# pp-ledger-registrar: the registrar's holders — creating and funding issued accounts.
add_executable(pp-ledger-registrar main.cpp)
target_link_libraries(pp-ledger-registrar PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-registrar)
pp_apps_app(pp-ledger-registrar registrar "PP Ledger Registrar" "Create pp-ledger accounts for their owners" system)
