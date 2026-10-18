# pp-ledger-reserve: the reserve's holders — transfers out of the treasury, made, signed, submitted.
add_executable(pp-ledger-reserve main.cpp)
target_link_libraries(pp-ledger-reserve PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-reserve)
pp_apps_app(pp-ledger-reserve reserve "PP Ledger Reserve" "Transfers out of a pp-ledger reserve" system)
