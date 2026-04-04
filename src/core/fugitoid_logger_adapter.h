#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void fugitoid_log_json(const char *level, const char *domain, const char *component,
                       const char *event, const char *correlation_id, const char *msg, const char *meta_json);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
inline void fugitoid_log_cpp(const std::string &level, const std::string &domain, const std::string &component,
                             const std::string &event, const std::string &cid, const std::string &msg, const std::string &meta) {
    fugitoid_log_json(level.c_str(), domain.c_str(), component.c_str(), event.c_str(), cid.c_str(), msg.c_str(), meta.c_str());
}
#endif
