#include "execution/vm/compilation_manager.h"
#include <tbb/task.h>

namespace noisepage::execution::vm {
// ---------------------------------------------------------
// Async Compile Task
// ---------------------------------------------------------

// This class encapsulates the ability to asynchronously JIT compile a module.
class CompilationManager::AsyncCompileTask : public tbb::task {
 public:
  // Construct an asynchronous compilation task to compile the the module
  explicit AsyncCompileTask(Module *module, int module_id, int region_id)
      : module_(module), module_id_(module_id), region_id_(region_id) {}

  // Execute
  tbb::task *execute() override {
    // start transactoin at time t2
    // This simply invokes Module::CompileToMachineCode() asynchronously.
    std::call_once(module_->compiled_flag_, [this]() {
      // Exit if the module has already been compiled. This might happen if
      // requested to execute in adaptive mode by concurrent threads.
      if (module_->jit_module_ != nullptr) {
        return;
      }

      // JIT the module.
      LLVMEngine::CompilerOptions options;
      module_->jit_module_ = LLVMEngine::Compile(*(module_->bytecode_module_), options);

      // JIT completed successfully. For each function in the module, pull out its
      // compiled implementation into the function cache, atomically replacing any
      // previous implementation.
      for (const auto &func_info : module_->bytecode_module_->GetFunctionsInfo()) {
        auto *jit_function = module_->jit_module_->GetFunctionPointer(func_info.GetName());
        NOISEPAGE_ASSERT(jit_function != nullptr, "Missing function in compiled module!");
        module_->functions_[func_info.GetId()].store(jit_function, std::memory_order_relaxed);
      }

      //TODO: want to look into deferred actions
      //module_map_.remove
    });
    // Done. There's no next task, so return null.
    return nullptr;
  }

 private:
  Module *module_;
  int module_id_;
  int region_id_;
};

CompilationManager::CompilationManager(common::ManagedPointer<transaction::TransactionManager> transaction_manager)
    : transaction_manager_(transaction_manager) {
  next_module_id_ = 0;
  next_region_id_ = 0;
}

void CompilationManager::AddModule(Module *module) {
  int module_id = std::atomic_load(&next_module_id_);
  module_map_[module_id] = nullptr;
  next_module_id_++;

  int region_id = std::atomic_load(&next_region_id_);
  region_map_[region_id] = nullptr;
  next_region_id_++;

  auto *compile_task = new (tbb::task::allocate_root()) AsyncCompileTask(module, module_id, region_id);
  tbb::task::enqueue(*compile_task);
}

void CompilationManager::TransferModule(std::unique_ptr<Module> &&module, int module_id) {
  if (module_map_.find(module_id) != module_map_.end()) {
    module_map_[module_id] = std::move(module);
  }
}

void CompilationManager::TransferRegion(std::unique_ptr<util::Region> region, int region_id) {
  if (region_map_.find(region_id) != region_map_.end()) {
    region_map_[region_id] = std::move(region);
  }
  // commit tranaction that started at t1
}


common::ManagedPointer<transaction::TransactionManager> CompilationManager::GetTransactionManager() {
  return transaction_manager_;
}

}  // namespace noisepage::execution::vm

