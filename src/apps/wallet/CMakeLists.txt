# pp-ledger-wallet: everyone's last-resort wallet — accounts proven from a pinned genesis hash.
add_executable(pp-ledger-wallet main.cpp)
target_link_libraries(pp-ledger-wallet PRIVATE apps_online)
pp_apps_apply_warnings(pp-ledger-wallet)
pp_apps_app(pp-ledger-wallet wallet "PP Ledger Wallet" "Your pp-ledger accounts, proven; move them when keys may have leaked" wallet)
