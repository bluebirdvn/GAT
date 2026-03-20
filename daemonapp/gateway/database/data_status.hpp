#ifndef DATA_STATUS_HPP
#define DATA_STATUS_HPP

typedef enum {
    DB_SUCCESS = 0,
    DB_FAILED,
    DB_NOT_FOUND,
    DB_DUPLICATE
} data_status_t;

#endif
