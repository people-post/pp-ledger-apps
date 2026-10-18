# pp-admin-admin: the admin set's holders — mint to the reserve, config updates, the admin set,
# beacon certificates.
add_executable(pp-admin-admin main.cpp)
target_link_libraries(pp-admin-admin PRIVATE admin_online)
pp_admin_apply_warnings(pp-admin-admin)
pp_admin_app(pp-admin-admin admin "PP Ledger Admin" "Mint, config updates, the admin set of a pp-ledger chain" admin)
