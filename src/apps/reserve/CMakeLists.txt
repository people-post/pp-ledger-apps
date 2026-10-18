# pp-admin-reserve: the reserve's holders — transfers out of the treasury, made, signed, submitted.
add_executable(pp-admin-reserve main.cpp)
target_link_libraries(pp-admin-reserve PRIVATE admin_online)
pp_admin_apply_warnings(pp-admin-reserve)
pp_admin_app(pp-admin-reserve reserve "PP Ledger Reserve" "Transfers out of a pp-ledger reserve" admin)
