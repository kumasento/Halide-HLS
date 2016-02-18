#include <iostream>
#include <limits>

#include "CodeGen_HLS_Testbench.h"
#include "CodeGen_Internal.h"
#include "Substitute.h"
#include "IROperator.h"
#include "Param.h"
#include "Var.h"
#include "Lerp.h"
#include "Simplify.h"

namespace Halide {
namespace Internal {

using std::ostream;
using std::endl;
using std::string;
using std::vector;
using std::pair;
using std::map;

class HLS_Closure : public Closure {
public:
    HLS_Closure(Stmt s)  {
        s.accept(this);
    }

    vector<HLS_Argument> arguments(const Scope<CodeGen_HLS_Base::Stencil_Type> &scope);

protected:
    using Closure::visit;

};


vector<HLS_Argument> HLS_Closure::arguments(const Scope<CodeGen_HLS_Base::Stencil_Type> &streams_scope) {
    vector<HLS_Argument> res;
    for (const pair<string, Closure::BufferRef> &i : buffers) {
        debug(3) << "buffer: " << i.first << " " << i.second.size;
        if (i.second.read) debug(3) << " (read)";
        if (i.second.write) debug(3) << " (write)";
        debug(3) << "\n";
    }
    internal_assert(buffers.empty()) << "we expect no references to buffers in a hw pipeline.\n";
    for (const pair<string, Type> &i : vars) {
        debug(3) << "var: " << i.first << "\n";
        if(ends_with(i.first, ".stream") ||
           ends_with(i.first, ".stencil") ) {
            CodeGen_HLS_Base::Stencil_Type stype = streams_scope.get(i.first);
            res.push_back({i.first, true, Type(), stype});
        } else if (ends_with(i.first, ".stencil_update")) {
            internal_error << "we don't expect to see a stencil_update type in HLS_Closure.\n";
        } else {
            // it is a scalar variable
            res.push_back({i.first, false, i.second, CodeGen_HLS_Base::Stencil_Type()});
        }
    }
    return res;
}

namespace {
const string hls_headers =
    "#include <hls_stream.h>\n"
    "#include \"Stencil.h\"\n"
    "#include \"hls_target.h\"\n";
}

CodeGen_HLS_Testbench::CodeGen_HLS_Testbench(ostream &tb_stream)
    : CodeGen_HLS_Base(tb_stream, false, "", hls_headers),
      cg_target("hls_target") {
    cg_target.init_module();
}

CodeGen_HLS_Testbench::~CodeGen_HLS_Testbench() {
}

void CodeGen_HLS_Testbench::visit(const ProducerConsumer *op) {
    if (starts_with(op->name, "_hls_target.")) {
        // steps over the start_hwacc() call
        const Block *block = op->produce.as<Block>();
        internal_assert(block);
        const Evaluate *eva = block->first.as<Evaluate>();
        internal_assert(eva);
        const Call *start_call = eva->value.as<Call>();
        internal_assert(start_call && start_call->name == "start_hwacc");

        Stmt hw_body = block->rest;

        debug(1) << "compute the closure for " << op->name << '\n';
        HLS_Closure c(hw_body);
        vector<HLS_Argument> args = c.arguments(stencils);

        // generate HLS target code using the child code generator
        cg_target.add_kernel(hw_body, op->name, args);

        do_indent();
        stream << "// produce " << op->name << '\n';

        // emits the target function call
        do_indent();
        stream << "p" << print_name(op->name) << "("; // avoid starting with '_'
        for(size_t i = 0; i < args.size(); i++) {
            stream << print_name(args[i].name);
            if(i != args.size() - 1)
                stream << ", ";
        }
        stream <<");\n";

        do_indent();
        stream << "// consume " << op->name << '\n';
        print_stmt(op->consume);
    } else {
        CodeGen_HLS_Base::visit(op);
    }
}
void CodeGen_HLS_Testbench::visit(const Allocate *op) {
    if (op->free_function == "_kernel_buffer") {
        // We only treat kernel buffer function differently in Zynq runtime.
        // Here, we just resets the new_expr and free_function
        // for this allocate node
        Stmt alloc = Allocate::make(op->name, op->type, op->extents, op->condition, op->body);
        alloc.accept(this);
    } else {
        CodeGen_HLS_Base::visit(op);
    }
}

void CodeGen_HLS_Testbench::visit(const Call *op) {
    if (op->name == "slice_kbuf" ||
        op->name == "create_kbuf") {
        // skips. this intrinsic call only used in Zynq codegen
        id = "0"; // skip evaluation
        return;
    } else {
        CodeGen_HLS_Base::visit(op);
    }
}

}
}