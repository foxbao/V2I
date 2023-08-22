#pragma once
#include <memory>
namespace civ
{
    namespace common
    {
        namespace util
        {

#define cr(T) T const &
#define sp(T) std::shared_ptr<T>
#define up(T) std::unique_ptr<T>
#define crsp_c(T) std::shared_ptr<T const> const &
#define sp_c(T) std::shared_ptr<T const>
#define crsp(T) std::shared_ptr<T> const &

#define DEFINE_EXTEND_TYPE(T)                           \
    using cr##T = T const &;                            \
    using sp##T = std::shared_ptr<T>;                   \
    using up##T = std::unique_ptr<T>;                   \
    using crsp_c##T = std::shared_ptr<T const> const &; \
    using sp_c##T = std::shared_ptr<T const>;           \
    using crsp##T = std::shared_ptr<T> const &

        } // namespace util
    }     // namespace common
} // namespace V2I
