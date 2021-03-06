#include "Func.h"
#include "Function.h"
#include "IR.h"
#include "IRMutator.h"
#include "Schedule.h"
#include "Var.h"

namespace Halide {

namespace Internal {

struct LoopLevelContents {
    mutable RefCount ref_count;

    // Note: func_ is empty for inline or root.
    std::string func_name;
    // TODO: these two fields should really be VarOrRVar,
    // but cyclical include dependencies make this challenging.
    std::string var_name;
    bool is_rvar;

    LoopLevelContents(const std::string &func_name,
                      const std::string &var_name,
                      bool is_rvar)
    : func_name(func_name), var_name(var_name), is_rvar(is_rvar) {}
};

template<>
EXPORT RefCount &ref_count<LoopLevelContents>(const LoopLevelContents *p) {
    return p->ref_count;
}

template<>
EXPORT void destroy<LoopLevelContents>(const LoopLevelContents *p) {
    delete p;
}

}  // namespace Internal

LoopLevel::LoopLevel(const std::string &func_name, const std::string &var_name, bool is_rvar)
    : contents(new Internal::LoopLevelContents(func_name, var_name, is_rvar)) {}

LoopLevel::LoopLevel(Internal::Function f, VarOrRVar v) : LoopLevel(f.name(), v.name(), v.is_rvar) {}

LoopLevel::LoopLevel(Func f, VarOrRVar v) : LoopLevel(f.function().name(), v.name(), v.is_rvar) {}

void LoopLevel::copy_from(const LoopLevel &other) {
    internal_assert(defined());
    contents->func_name = other.contents->func_name;
    contents->var_name = other.contents->var_name;
    contents->is_rvar = other.contents->is_rvar;
}

bool LoopLevel::defined() const {
    return contents.defined();
}

std::string LoopLevel::func() const {
    internal_assert(defined());
    return contents->func_name;
}

VarOrRVar LoopLevel::var() const {
    internal_assert(defined());
    internal_assert(!is_inline() && !is_root());
    return VarOrRVar(contents->var_name, contents->is_rvar);
}

/*static*/
LoopLevel LoopLevel::inlined() {
    return LoopLevel("", "", false);
}

bool LoopLevel::is_inline() const {
    internal_assert(defined());
    return contents->var_name.empty();
}

/*static*/
LoopLevel LoopLevel::root() {
    return LoopLevel("", "__root", false);
}

bool LoopLevel::is_root() const {
    internal_assert(defined());
    return contents->var_name == "__root";
}

std::string LoopLevel::to_string() const {
    internal_assert(defined());
    return contents->func_name + "." + contents->var_name;
}

bool LoopLevel::match(const std::string &loop) const {
    internal_assert(defined());
    return Internal::starts_with(loop, contents->func_name + ".") &&
           Internal::ends_with(loop, "." + contents->var_name);
}

bool LoopLevel::match(const LoopLevel &other) const {
    internal_assert(defined());
    return (contents->func_name == other.contents->func_name &&
            (contents->var_name == other.contents->var_name ||
             Internal::ends_with(contents->var_name, "." + other.contents->var_name) ||
             Internal::ends_with(other.contents->var_name, "." + contents->var_name)));
}

bool LoopLevel::operator==(const LoopLevel &other) const {
    return defined() == other.defined() &&
           contents->func_name == other.contents->func_name &&
           contents->var_name == other.contents->var_name;
}

namespace Internal {

typedef std::map<IntrusivePtr<FunctionContents>, IntrusivePtr<FunctionContents>> DeepCopyMap;

IntrusivePtr<FunctionContents> deep_copy_function_contents_helper(
    const IntrusivePtr<FunctionContents> &src,
    DeepCopyMap &copied_map);

/** A schedule for a halide function, which defines where, when, and
 * how it should be evaluated. */
struct FuncScheduleContents {
    mutable RefCount ref_count;

    LoopLevel store_level, compute_level;
    std::vector<StorageDim> storage_dims;
    std::vector<Bound> bounds;
    std::map<std::string, IntrusivePtr<Internal::FunctionContents>> wrappers;
    bool memoized;

    //----- HLS Modification Begins -----//
    // TODO(jingpu) move it to StageSchedule
    bool is_hw_kernel;   // TODO equivalent to !accelerate_exit.empty()
    bool is_accelerated;  // TODO equivalent to !accelerate_input.empty()
    bool is_linebuffered;
    std::set<std::string> accelerate_inputs;
    std::string accelerate_exit;
    LoopLevel accelerate_compute_level, accelerate_store_level;
    std::map<std::string, int> fifo_depths;   // key is the name of the consumer
    bool is_kernel_buffer;
    bool is_kernel_buffer_slice;
    std::map<std::string, Function> tap_funcs;
    std::map<std::string, Parameter> tap_params;
    //----- HLS Modification Ends -------//

    FuncScheduleContents()
        : store_level(LoopLevel::inlined()),
          compute_level(LoopLevel::inlined()), memoized(false),
          //----- HLS Modification Begins -----//
          is_hw_kernel(false), is_accelerated(false), is_linebuffered(false),
          is_kernel_buffer(false), is_kernel_buffer_slice(false){};
          //----- HLS Modification Ends -------//

    // Pass an IRMutator through to all Exprs referenced in the FuncScheduleContents
    void mutate(IRMutator *mutator) {
        for (Bound &b : bounds) {
            if (b.min.defined()) {
                b.min = mutator->mutate(b.min);
            }
            if (b.extent.defined()) {
                b.extent = mutator->mutate(b.extent);
            }
            if (b.modulus.defined()) {
                b.modulus = mutator->mutate(b.modulus);
            }
            if (b.remainder.defined()) {
                b.remainder = mutator->mutate(b.remainder);
            }
        }
    }
};

template<>
EXPORT RefCount &ref_count<FuncScheduleContents>(const FuncScheduleContents *p) {
    return p->ref_count;
}

template<>
EXPORT void destroy<FuncScheduleContents>(const FuncScheduleContents *p) {
    delete p;
}


/** A schedule for a sigle halide stage, which defines where, when, and
 * how it should be evaluated. */
struct StageScheduleContents {
    mutable RefCount ref_count;

    std::vector<ReductionVariable> rvars;
    std::vector<Split> splits;
    std::vector<Dim> dims;
    std::vector<PrefetchDirective> prefetches;
    bool touched;
    bool allow_race_conditions;

    StageScheduleContents() : touched(false), allow_race_conditions(false) {};

    // Pass an IRMutator through to all Exprs referenced in the StageScheduleContents
    void mutate(IRMutator *mutator) {
        for (ReductionVariable &r : rvars) {
            if (r.min.defined()) {
                r.min = mutator->mutate(r.min);
            }
            if (r.extent.defined()) {
                r.extent = mutator->mutate(r.extent);
            }
        }
        for (Split &s : splits) {
            if (s.factor.defined()) {
                s.factor = mutator->mutate(s.factor);
            }
        }
        for (PrefetchDirective &p : prefetches) {
            if (p.offset.defined()) {
                p.offset = mutator->mutate(p.offset);
            }
        }
    }
};

template<>
EXPORT RefCount &ref_count<StageScheduleContents>(const StageScheduleContents *p) {
    return p->ref_count;
}

template<>
EXPORT void destroy<StageScheduleContents>(const StageScheduleContents *p) {
    delete p;
}


FuncSchedule::FuncSchedule() : contents(new FuncScheduleContents) {}

FuncSchedule FuncSchedule::deep_copy(
        std::map<IntrusivePtr<FunctionContents>, IntrusivePtr<FunctionContents>> &copied_map) const {

    internal_assert(contents.defined()) << "Cannot deep-copy undefined FuncSchedule\n";
    FuncSchedule copy;
    copy.contents->store_level = contents->store_level;
    copy.contents->compute_level = contents->compute_level;
    copy.contents->storage_dims = contents->storage_dims;
    copy.contents->bounds = contents->bounds;
    copy.contents->memoized = contents->memoized;

    //----- HLS Modification Begins -----//
    // HLS related fields
    copy.contents->is_hw_kernel = contents->is_hw_kernel;
    copy.contents->is_accelerated = contents->is_accelerated;
    copy.contents->is_linebuffered = contents->is_linebuffered;
    copy.contents->accelerate_inputs = contents->accelerate_inputs;
    copy.contents->accelerate_exit = contents->accelerate_exit;
    copy.contents->accelerate_compute_level = contents->accelerate_compute_level;
    copy.contents->accelerate_store_level = contents->accelerate_store_level;
    copy.contents->fifo_depths = contents->fifo_depths;
    copy.contents->is_kernel_buffer = contents->is_kernel_buffer;
    copy.contents->is_kernel_buffer_slice = contents->is_kernel_buffer_slice;
    copy.contents->tap_funcs = contents->tap_funcs;
    copy.contents->tap_params = contents->tap_params;
    //----- HLS Modification Ends -------//

    // Deep-copy wrapper functions. If function has already been deep-copied before,
    // i.e. it's in the 'copied_map', use the deep-copied version from the map instead
    // of creating a new deep-copy
    for (const auto &iter : contents->wrappers) {
        IntrusivePtr<FunctionContents> &copied_func = copied_map[iter.second];
        if (copied_func.defined()) {
            copy.contents->wrappers[iter.first] = copied_func;
        } else {
            copy.contents->wrappers[iter.first] = deep_copy_function_contents_helper(iter.second, copied_map);
            copied_map[iter.second] = copy.contents->wrappers[iter.first];
        }
    }
    internal_assert(copy.contents->wrappers.size() == contents->wrappers.size());
    return copy;
}

bool &FuncSchedule::memoized() {
    return contents->memoized;
}

bool FuncSchedule::memoized() const {
    return contents->memoized;
}

std::vector<StorageDim> &FuncSchedule::storage_dims() {
    return contents->storage_dims;
}

const std::vector<StorageDim> &FuncSchedule::storage_dims() const {
    return contents->storage_dims;
}

std::vector<Bound> &FuncSchedule::bounds() {
    return contents->bounds;
}

const std::vector<Bound> &FuncSchedule::bounds() const {
    return contents->bounds;
}

std::map<std::string, IntrusivePtr<Internal::FunctionContents>> &FuncSchedule::wrappers() {
    return contents->wrappers;
}

const std::map<std::string, IntrusivePtr<Internal::FunctionContents>> &FuncSchedule::wrappers() const {
    return contents->wrappers;
}

void FuncSchedule::add_wrapper(const std::string &f,
                               const IntrusivePtr<Internal::FunctionContents> &wrapper) {
    if (contents->wrappers.count(f)) {
        if (f.empty()) {
            user_warning << "Replacing previous definition of global wrapper in function \""
                         << f << "\"\n";
        } else {
            internal_error << "Wrapper redefinition in function \"" << f << "\" is not allowed\n";
        }
    }
    contents->wrappers[f] = wrapper;
}

LoopLevel &FuncSchedule::store_level() {
    return contents->store_level;
}

LoopLevel &FuncSchedule::compute_level() {
    return contents->compute_level;
}

const LoopLevel &FuncSchedule::store_level() const {
    return contents->store_level;
}

const LoopLevel &FuncSchedule::compute_level() const {
    return contents->compute_level;
}

void FuncSchedule::accept(IRVisitor *visitor) const {
    for (const Bound &b : bounds()) {
        if (b.min.defined()) {
            b.min.accept(visitor);
        }
        if (b.extent.defined()) {
            b.extent.accept(visitor);
        }
        if (b.modulus.defined()) {
            b.modulus.accept(visitor);
        }
        if (b.remainder.defined()) {
            b.remainder.accept(visitor);
        }
    }
}

void FuncSchedule::mutate(IRMutator *mutator) {
    if (contents.defined()) {
        contents->mutate(mutator);
    }
}


StageSchedule::StageSchedule() : contents(new StageScheduleContents) {}

StageSchedule StageSchedule::get_copy() const {
    internal_assert(contents.defined()) << "Cannot copy undefined Schedule\n";
    StageSchedule copy;
    copy.contents->rvars = contents->rvars;
    copy.contents->splits = contents->splits;
    copy.contents->dims = contents->dims;
    copy.contents->prefetches = contents->prefetches;
    copy.contents->touched = contents->touched;
    copy.contents->allow_race_conditions = contents->allow_race_conditions;
    return copy;
}

bool &StageSchedule::touched() {
    return contents->touched;
}

bool StageSchedule::touched() const {
    return contents->touched;
}

std::vector<ReductionVariable> &StageSchedule::rvars() {
    return contents->rvars;
}

const std::vector<ReductionVariable> &StageSchedule::rvars() const {
    return contents->rvars;
}

const std::vector<Split> &StageSchedule::splits() const {
    return contents->splits;
}

std::vector<Split> &StageSchedule::splits() {
    return contents->splits;
}

std::vector<Dim> &StageSchedule::dims() {
    return contents->dims;
}

const std::vector<Dim> &StageSchedule::dims() const {
    return contents->dims;
}

std::vector<PrefetchDirective> &StageSchedule::prefetches() {
    return contents->prefetches;
}

const std::vector<PrefetchDirective> &StageSchedule::prefetches() const {
    return contents->prefetches;
}

//----- HLS Modification Begins -----//
bool &FuncSchedule::is_hw_kernel() {
    return contents->is_hw_kernel;
}

bool FuncSchedule::is_hw_kernel() const {
    return contents->is_hw_kernel;
}

bool FuncSchedule::is_accelerated() const {
    return contents->is_accelerated;
}

bool &FuncSchedule::is_accelerated() {
    return contents->is_accelerated;
}

bool FuncSchedule::is_linebuffered() const {
    return contents->is_linebuffered;
}

bool &FuncSchedule::is_linebuffered() {
    return contents->is_linebuffered;
}

bool FuncSchedule::is_kernel_buffer() const {
    return contents->is_kernel_buffer;
}

bool &FuncSchedule::is_kernel_buffer() {
    return contents->is_kernel_buffer;
}

bool FuncSchedule::is_kernel_buffer_slice() const {
    return contents->is_kernel_buffer_slice;
}

bool &FuncSchedule::is_kernel_buffer_slice() {
    return contents->is_kernel_buffer_slice;
}

const std::set<std::string> &FuncSchedule::accelerate_inputs() const{
    return contents->accelerate_inputs;
}

std::set<std::string> &FuncSchedule::accelerate_inputs() {
    return contents->accelerate_inputs;
}

const std::map<std::string, Function> &FuncSchedule::tap_funcs() const {
    return contents->tap_funcs;
}

std::map<std::string, Function> &FuncSchedule::tap_funcs() {
    return contents->tap_funcs;
}

const std::map<std::string, Parameter> &FuncSchedule::tap_params() const {
    return contents->tap_params;
}

std::map<std::string, Parameter> &FuncSchedule::tap_params() {
    return contents->tap_params;
}

const std::map<std::string, int> &FuncSchedule::fifo_depths() const {
    return contents->fifo_depths;
}

std::map<std::string, int> &FuncSchedule::fifo_depths() {
    return contents->fifo_depths;
}

const std::string &FuncSchedule::accelerate_exit() const{
    return contents->accelerate_exit;
}

std::string &FuncSchedule::accelerate_exit() {
    return contents->accelerate_exit;
}

LoopLevel &FuncSchedule::accelerate_compute_level() {
    internal_assert(is_accelerated());
    return contents->accelerate_compute_level;
}

const LoopLevel &FuncSchedule::accelerate_compute_level() const {
    internal_assert(is_accelerated());
    return contents->accelerate_compute_level;
}

LoopLevel &FuncSchedule::accelerate_store_level() {
    internal_assert(is_accelerated());
    return contents->accelerate_store_level;
}

const LoopLevel &FuncSchedule::accelerate_store_level() const {
    internal_assert(is_accelerated());
    return contents->accelerate_store_level;
}
//----- HLS Modification Ends -------//

bool &StageSchedule::allow_race_conditions() {
    return contents->allow_race_conditions;
}

bool StageSchedule::allow_race_conditions() const {
    return contents->allow_race_conditions;
}

void StageSchedule::accept(IRVisitor *visitor) const {
    for (const ReductionVariable &r : rvars()) {
        if (r.min.defined()) {
            r.min.accept(visitor);
        }
        if (r.extent.defined()) {
            r.extent.accept(visitor);
        }
    }
    for (const Split &s : splits()) {
        if (s.factor.defined()) {
            s.factor.accept(visitor);
        }
    }
    for (const PrefetchDirective &p : prefetches()) {
        if (p.offset.defined()) {
            p.offset.accept(visitor);
        }
    }
}

void StageSchedule::mutate(IRMutator *mutator) {
    if (contents.defined()) {
        contents->mutate(mutator);
    }
}

}  // namespace Internal
}  // namespace Halide
