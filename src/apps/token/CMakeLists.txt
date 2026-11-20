# pp-ledger-token: a token issuer's operators — its overview, issuing, sending, its signers.
add_executable(pp-ledger-token main.cpp)
target_link_libraries(pp-ledger-token PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-token)
pp_apps_app(pp-ledger-token token "PP Ledger Token" "Issue and manage a pp-ledger token" token)
