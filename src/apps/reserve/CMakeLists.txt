# pp-admin-reserve: the reserve's holders — transfers out of the treasury, made, signed, submitted.
add_executable(pp-admin-reserve main.cpp)
target_link_libraries(pp-admin-reserve PRIVATE admin_online)
pp_admin_apply_warnings(pp-admin-reserve)
