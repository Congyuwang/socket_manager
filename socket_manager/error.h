#define CHECK_RET(ret, err)                                                    \
  if (0 != ret) [[unlikely]] {                                                 \
    const std::string err_str(err);                                            \
    free(err);                                                                 \
    throw std::runtime_error(err_str);                                         \
  }
