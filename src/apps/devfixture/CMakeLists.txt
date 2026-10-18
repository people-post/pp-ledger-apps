# Development only: a vault with keys and people, and operations to review, for screenshots.
add_executable(pp-ledger-devfixture main.cpp)
target_link_libraries(pp-ledger-devfixture PRIVATE apps_core pp::chain)
pp_apps_apply_warnings(pp-ledger-devfixture)
