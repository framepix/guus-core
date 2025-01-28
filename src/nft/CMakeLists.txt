add_library(nft_db
    nft_db.cpp
)

target_link_libraries(nft_db
    PRIVATE
        cryptonote_core
        sqlite3
)
