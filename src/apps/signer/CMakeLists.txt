# pp-ledger-signer: review, sign and merge operations, offline (AD005: it links no network code).
add_executable(pp-ledger-signer main.cpp)
target_link_libraries(pp-ledger-signer PRIVATE apps_ui)
pp_apps_apply_warnings(pp-ledger-signer)
pp_apps_app(pp-ledger-signer signer "PP Ledger Signer" "Review and sign pp-ledger operations, offline" signer)
