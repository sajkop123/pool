#include <vector>


/**
 * The most simple pool, just have memory allocation capability
 *
 * Not support
 *  - dynamic pool size
 *  - thread-safe
 *
 * support
 *  - single producer, single consumer
 */
template<typename T>
class Pool {
 public:
  T* acquire();
  void recycle(T*);

 private:
  pool_config m_config;
  std::vector<T> m_data;
  std::vector<atomic_bool> m_status;
};

template<typename T>
T* Pool<T>::acquire() {

}