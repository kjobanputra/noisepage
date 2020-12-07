#pragma once

#include <tbb/concurrent_unordered_map.h>

#include <string>
#include <unordered_map>

#include "execution/ast/context.h"
#include "transaction/transaction_manager.h"
#include "module.h"

namespace noisepage::execution::vm {
/**
 * A CompilationManager instance will handle asynchronous compilation tasks and
 * return back a handle to the user of the class.
 */
class CompilationManager {
 public:
  CompilationManager(common::ManagedPointer<transaction::TransactionManager> transaction_manager);
  ~CompilationManager() = default;

  // Send a module to the compilation manager for compilation.
  void AddModule(Module *module);

  void TransferModule(std::unique_ptr<Module> &&module, int module_id);

  void TransferRegion(std::unique_ptr<util::Region> region, int region_id);

  common::ManagedPointer<transaction::TransactionManager> GetTransactionManager();

 private:
  class AsyncCompileTask;
 protected:
  std::vector<std::unique_ptr<Module>> module_;
  std::vector<std::unique_ptr<util::Region>> region_;
  std::atomic<int> next_module_id_;
  std::atomic<int> next_region_id_;
  static tbb::concurrent_unordered_map<std::atomic<int>, std::unique_ptr<Module>> module_map_;
  static tbb::concurrent_unordered_map<std::atomic<int>, std::unique_ptr<util::Region>> region_map_;
  common::ManagedPointer<transaction::TransactionManager> transaction_manager_;
};
}
