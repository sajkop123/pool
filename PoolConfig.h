#include "common.h"


/**
 * Pool internal operation status
 */
enum class pool_op_status {
  success,
  busy,
  empty,
  full,
};

/**
 * We could not expect there always available resource when user acquire
 * here we provide some action pool can support when resource is exhausted.
 */
enum class exhaust_action {
  /**
   * blocking user acquire behavior until available resource appears
   */
  wait,

  /**
   * autimatically allocate new resource
   *
   * @warning the grow implementation can not satisfy all pool using scenario.
   */
  grow,
};

enum class ps_type {
  spsc,  /* single producer, single consumer */
  mpsc,  /* multiple producer, single consumer */
  spmc,  /* single producer, multiple consumer */
  mpmc,  /* multiple producer, multiple consumer */
};

/**
 * To reach better performance, some user spec (or said promise) is necessary
 * for pool.
 */
struct user_spec {
  /**
   * user promise acquired resource always be recycled in FIFO.
   */
  bool is_recycle_FIFO;

  /**
   * make sure what kind of producer-consumer the pool user is.
   *
   * @warning currently ps_type::spmc && spmc::mpmc are not support.
   *          which means twice or more times of recyling call of a single
   *          object is not admitted.
   */
  ps_type ps_type;

};

struct pool_config {
  /**
   * initial pool size when pool object is created.
   */
  size_t pool_size;

  /**
   *
   */
  user_spec user_spec;

  /**
   * software flexibility for user to decide what behavior the pool is expected
   * to do when avaialble resource is exhausted.
   */
  exhaust_action exhaust_action;
};