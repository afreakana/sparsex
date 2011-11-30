/*
 * jit.cc -- Just In Time compilation routines.
 *
 * Copyright (C) 2011, Computing Systems Laboratory (CSLab), NTUA.
 * Copyright (C) 2011, Vasileios Karakasis
 * All rights reserved.
 *
 * This file is distributed under the BSD License. See LICENSE.txt for details.
 */
#include <iostream>
#include <sstream>
#include <cassert>

#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetSelect.h"
#include "llvm/Support/StandardPasses.h"

#include "spm.h"
#include "csx.h"
#include "drle.h"
#include "jit.h"
#include "jit_config.h"
#include "jit_util.h"

using namespace llvm;
using namespace csx;

CsxExecutionEngine &csx::CsxJitInit(void)
{
    InitializeNativeTarget();
    return CsxExecutionEngine::CreateEngine();
}

CsxJit::CsxJit(CsxManager *csxmg, CsxExecutionEngine *engine, unsigned int tid)
    : csxmg_(csxmg), module_(0), engine_(engine),
      thread_id_(tid)
{
    context_ = new LLVMContext();
    compiler_ = new ClangCompiler();
};


TemplateText *CsxJit::GetMultTemplate(SpmIterOrder type)
{
    TemplateText *ret;

    if (mult_templates_.count(type)) {
        ret = mult_templates_[type];
        goto exit;
    }

    switch (type) {
    case NONE:
        ret = new TemplateText(SourceFromFile(DeltaTemplateSource));
        mult_templates_[type] = ret;
        break;
    case HORIZONTAL:
        ret = new TemplateText(SourceFromFile(HorizTemplateSource));
        mult_templates_[type] = ret;
        break;
    case VERTICAL:
        ret = new TemplateText(SourceFromFile(VertTemplateSource));
        mult_templates_[type] = ret;
        break;
    case DIAGONAL:
        ret = new TemplateText(SourceFromFile(DiagTemplateSource));
        mult_templates_[type] = ret;
        break;
    case REV_DIAGONAL:
        ret = new TemplateText(SourceFromFile(RDiagTemplateSource));
        mult_templates_[type] = ret;
        break;
    default:
        assert(0 && "unknown pattern type");
    };

exit:
    return ret;
}

std::string CsxJit::DoGenDeltaCase(int delta_bits)
{
    TemplateText *tmpl = GetMultTemplate(NONE);
    std::map<std::string, std::string> subst_map;
    subst_map["bits"] = Stringify(delta_bits);
    int delta_bytes = delta_bits / 8;
    if (delta_bytes > 1)
        subst_map["align_ctl"] =
            "align_ptr(ctl," + Stringify(delta_bytes) + ");";

    return tmpl->Substitute(subst_map);
}

std::string CsxJit::DoGenHorizCase(int delta)
{
    TemplateText *tmpl = GetMultTemplate(HORIZONTAL);
    std::map<std::string, std::string> subst_map;
    subst_map["delta"] = Stringify(delta);
    return tmpl->Substitute(subst_map);
}

std::string CsxJit::DoGenVertCase(int delta)
{
    TemplateText *tmpl = GetMultTemplate(VERTICAL);
    std::map<std::string, std::string> subst_map;
    subst_map["delta"] = Stringify(delta);
    return tmpl->Substitute(subst_map);
}

std::string CsxJit::DoGenDiagCase(int delta)
{
    TemplateText *tmpl = GetMultTemplate(DIAGONAL);
    std::map<std::string, std::string> subst_map;
    subst_map["delta"] = Stringify(delta);
    return tmpl->Substitute(subst_map);
}

std::string CsxJit::DoGenRDiagCase(int delta)
{
    TemplateText *tmpl = GetMultTemplate(REV_DIAGONAL);
    std::map<std::string, std::string> subst_map;
    subst_map["delta"] = Stringify(delta);
    return tmpl->Substitute(subst_map);
}

void CsxJit::DoNewRowHook(std::map<std::string, std::string> &hooks,
                          std::ostream &log, bool rowjmp) const
{
    if (rowjmp) {
        hooks["new_row_hook"] =
            "if (test_bit(&flags, CTL_RJMP_BIT))\n"
            "\t\t\t\ty_curr += ul_get(&ctl);\n"
            "\t\t\telse\n"
            "\t\t\t\ty_curr++;";
    } else {
        hooks["new_row_hook"] = "y_curr++;";
    };
}

void CsxJit::DoSpmvFnHook(std::map<std::string, std::string> &hooks,
                          std::ostream &log)
{
    CsxManager::PatMap::iterator i_patt = csxmg_->patterns.begin();
    CsxManager::PatMap::iterator i_patt_end = csxmg_->patterns.end();
    std::map<long, std::string> func_entries;

    uint64_t delta;
    for (; i_patt != i_patt_end; ++i_patt) {
        std::string patt_code, patt_func_entry;
        long patt_id = i_patt->second.flag;
        SpmIterOrder type =
            static_cast<SpmIterOrder>(i_patt->first / CSX_PID_OFFSET);
        delta = i_patt->first % CSX_PID_OFFSET;
        switch (type) {
        case NONE:
            assert(delta ==  8 ||
                   delta == 16 ||
                   delta == 32 ||
                   delta == 64);
            log << "type:DELTA size:" << delta << " nnz:"
                << i_patt ->second.nr << "\n";
            patt_code = DoGenDeltaCase(delta);
            patt_func_entry = "delta" + Stringify(delta) + "_case";
            break;
        case HORIZONTAL:
            log << "type:HORIZONTAL delta:" << delta
                << " nnz:" << i_patt->second.nr << std::endl;
            patt_code = DoGenHorizCase(delta);
            patt_func_entry = "horiz" + Stringify(delta) + "_case";
            break;
        case VERTICAL:
            log << "type:VERTICAL delta:" << delta
                << " nnz:" << i_patt->second.nr << std::endl;
            patt_code = DoGenVertCase(delta);
            patt_func_entry = "vert" + Stringify(delta) + "_case";
            break;
        case DIAGONAL:
            log << "type:DIAGONAL delta:" << delta
                << " nnz:" << i_patt->second.nr << std::endl;
            patt_code = DoGenDiagCase(delta);
            patt_func_entry = "diag" + Stringify(delta) + "_case";
            break;
	case REV_DIAGONAL:
            log << "type:DIAGONAL delta:" << delta
                << " nnz:" << i_patt->second.nr << std::endl;
            patt_code = DoGenRDiagCase(delta);
            patt_func_entry = "rdiag" + Stringify(delta) + "_case";
            break;
        default:
            assert(0 && "unknown type");
        }

        hooks["spmv_func_definitions"] += patt_code + "\n";
        func_entries[patt_id] = patt_func_entry;
    }

    // Add function table entries in the correct order
    std::map<long, std::string>::const_iterator i_fentry =
        func_entries.begin();
    std::map<long, std::string>::const_iterator fentries_end =
        func_entries.end();

    if (func_entries.size() == 1) {
        // Don't switch, just call the pattern-specific mult. routine
        hooks["spmv_func_entries"] += "\t" + i_fentry->second + ",\n";
        hooks["body_hook"] =
            "yr += mult_table[" + Stringify(i_fentry->first) + "]"
            "(&ctl, size, &v, &x_curr, &y_curr);";
    } else {
        hooks["body_hook"] = "switch (patt_id) {\n";
        for (; i_fentry != fentries_end; ++i_fentry) {
            hooks["spmv_func_entries"] += "\t" + i_fentry->second + ",\n";
            hooks["body_hook"] +=
                "\t\tcase " + Stringify(i_fentry->first) + ":\n"
                "\t\t\tyr += mult_table[" + Stringify(i_fentry->first) + "]"
                "(&ctl, size, &v, &x_curr, &y_curr);\n"
                "\t\t\tbreak;\n";
        }
    
        // Add default statement
        hooks["body_hook"] += "\t\tdefault:\n"
            "\t\t\tfprintf(stderr, \"[BUG] unknown pattern\\n\");\n"
            "\t\t\texit(1);\n"
            "\t\t};";
    }
}

void CsxJit::GenCode(std::ostream &log)
{
    // Load the template source
    TemplateText source_tmpl(SourceFromFile(CsxTemplateSource));

    // Fill in the hooks
    std::map<std::string, std::string> hooks;
    DoNewRowHook(hooks, log, csxmg_->HasRowJmps());
    DoSpmvFnHook(hooks, log);

    // Substitute and compile into an LLVM module
    compiler_->SetLogStream(&log);
    // compiler_->SetDebugMode(true);
    module_ = DoCompile(source_tmpl.Substitute(hooks));
    if (!module_) {
        log << "compilation failed for thread " << thread_id_ << "\n";
        exit(1);
    }
}

Module *CsxJit::DoCompile(const std::string &source) const
{
    if (compiler_->DebugMode())
        compiler_->KeepTemporaries(true);
    return compiler_->Compile(source, context_);
}

void CsxJit::DoOptimizeModule()
{
    PassManager pm;
    pm.add(new TargetData(module_));
    createStandardModulePasses(
        &pm,
        /* -O4 */ 4,
        /* OptimizeSize */ false,
        /* UnitAtATime      */  true,
        /* UnrollLoops      */  true,
        /* SimplifyLibCalls */  true,
        /* HaveExceptions   */  false,
        /* InliningPass     */ createFunctionInliningPass());
                               
    createStandardLTOPasses(
        &pm,
        /* Internalize     */ true,
        /* RunInliner      */ true,
        /* VerifyEach      */ false);

    pm.run(*module_);
    return;
}

void *CsxJit::GetSpmvFn() const
{
    engine_->AddModule(module_);
    Function *llvm_fn = module_->getFunction("spm_csx32_double_multiply");
    assert(llvm_fn);
    return engine_->GetPointerToFunction(llvm_fn);
}

void CsxExecutionEngine::AddModule(Module *mod)
{
    if (!llvm_engine_) {
        std::string errmsg;
        llvm_engine_ = ExecutionEngine::createJIT(mod, &errmsg);
        if (!llvm_engine_) {
            std::cerr << "failed to create LLVM execution engine: "
                      << errmsg << "\n";
            exit(1);
        }
    } else {
        llvm_engine_->addModule(mod);
    }
}

#if 0
int main(int argc, char **argv)
{
    CsxManager csxmg(NULL);
    CsxJit jit(&csxmg);
    jit.GenCode(std::cout);
    return 0;
}
#endif

// vim:expandtab:tabstop=8:shiftwidth=4:softtabstop=4
