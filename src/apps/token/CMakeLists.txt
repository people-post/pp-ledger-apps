# pp-admin-token: a token issuer's operators — its overview, issuing, sending, its signers.
add_executable(pp-admin-token main.cpp)
target_link_libraries(pp-admin-token PRIVATE admin_online)
pp_admin_apply_warnings(pp-admin-token)
pp_admin_app(pp-admin-token token "PP Ledger Token" "Issue and manage a pp-ledger token" token)
