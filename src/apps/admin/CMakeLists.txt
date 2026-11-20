# pp-ledger-admin: the admin set's holders — mint to the reserve, config updates, the admin set,
# beacon certificates.
add_executable(pp-ledger-admin main.cpp)
target_link_libraries(pp-ledger-admin PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-admin)
pp_apps_app(pp-ledger-admin admin "PP Ledger Admin" "Mint, config updates, the admin set of a pp-ledger chain" system)
