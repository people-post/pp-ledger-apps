# Development only: a vault with keys and people, and operations to review, for screenshots.
add_executable(pp-admin-devfixture main.cpp)
target_link_libraries(pp-admin-devfixture PRIVATE admin_core)
pp_admin_apply_warnings(pp-admin-devfixture)
