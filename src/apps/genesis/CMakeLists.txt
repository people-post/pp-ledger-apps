# pp-ledger-genesis: the people who start the chain and hold its recovery keys — the genesis plan,
# signed offline, and recovery announcements.
add_executable(pp-ledger-genesis main.cpp)
target_link_libraries(pp-ledger-genesis PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-genesis)
pp_apps_app(pp-ledger-genesis genesis "PP Ledger Genesis" "Plan a pp-ledger chain's start; recovery" system)
