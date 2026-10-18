# pp-admin-genesis: the people who start the chain and hold its recovery keys — the genesis plan,
# signed offline, and recovery announcements.
add_executable(pp-admin-genesis main.cpp)
target_link_libraries(pp-admin-genesis PRIVATE admin_online)
pp_admin_apply_warnings(pp-admin-genesis)
