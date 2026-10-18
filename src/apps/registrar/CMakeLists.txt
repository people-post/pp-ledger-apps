# pp-admin-registrar: the registrar's holders — creating and funding issued accounts.
add_executable(pp-admin-registrar main.cpp)
target_link_libraries(pp-admin-registrar PRIVATE admin_online)
pp_admin_apply_warnings(pp-admin-registrar)
