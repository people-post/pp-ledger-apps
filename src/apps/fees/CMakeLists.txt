# pp-ledger-fees: the fee account's holders — rounds that hand the collected fees on to miners and relays.
add_executable(pp-ledger-fees main.cpp)
target_link_libraries(pp-ledger-fees PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-fees)
pp_apps_app(pp-ledger-fees fees "PP Ledger Fees" "Hand a pp-ledger chain's fees on to its miners and relays" system)
