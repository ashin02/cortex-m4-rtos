#include <stdbool.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#include "aerortos/cortex_m4.h"
#include "aerortos/critical_section.h"
#include "aerortos/monitor.h"
#include "aerortos/mutex.h"
#include "aerortos/platform.h"
#include "aerortos/port.h"
#include "aerortos/queue.h"
#include "aerortos/scheduler.h"
#include "aerortos/task.h"
#include "aerortos/timer.h"

#define M10_STACK_WORDS 224U
#define M10_COMPLETE_MAGIC UINT32_C(0x4D313041)
#define M10_PARTICIPANT_COUNT 6U
#define M10_PERIODIC_SAMPLES 8U
#define M10_OVERHEAD_SAMPLES 32U
#define M10_EXPECTED_PASS_COUNT 80U

volatile uint32_t aerortos_m10_hw_pass_count;
volatile uint32_t aerortos_m10_hw_fail_count;
volatile uint32_t aerortos_m10_hw_complete;
volatile uint32_t aerortos_m10_hw_expected_pass_count =
    M10_EXPECTED_PASS_COUNT;
volatile uint32_t aerortos_m10_hw_participant_count;
volatile uint32_t aerortos_m10_hw_voluntary_yield_count;
volatile uint32_t aerortos_m10_hw_queue_operation_count;
volatile uint32_t aerortos_m10_hw_mutex_handoff_count;
volatile uint32_t aerortos_m10_hw_periodic_job_count;

volatile uint32_t aerortos_m10_high_complete;
volatile uint32_t aerortos_m10_consumer_complete;
volatile uint32_t aerortos_m10_peer_a_complete;
volatile uint32_t aerortos_m10_peer_b_complete;
volatile uint32_t aerortos_m10_low_complete;
volatile uint32_t aerortos_m10_periodic_complete;

aerortos_queue_t aerortos_m10_queue;
aerortos_mutex_t aerortos_m10_mutex;
aerortos_task_t *aerortos_m10_idle_task;
aerortos_task_t *aerortos_m10_high_task;
aerortos_task_t *aerortos_m10_periodic_task;
aerortos_task_t *aerortos_m10_consumer_task;
aerortos_task_t *aerortos_m10_peer_a_task;
aerortos_task_t *aerortos_m10_peer_b_task;
aerortos_task_t *aerortos_m10_low_task;

alignas(8) static uint32_t idle_stack[M10_STACK_WORDS];
alignas(8) static uint32_t high_stack[M10_STACK_WORDS];
alignas(8) static uint32_t periodic_stack[M10_STACK_WORDS];
alignas(8) static uint32_t consumer_stack[M10_STACK_WORDS];
alignas(8) static uint32_t peer_a_stack[M10_STACK_WORDS];
alignas(8) static uint32_t peer_b_stack[M10_STACK_WORDS];
alignas(8) static uint32_t low_stack[M10_STACK_WORDS];
static uint32_t queue_storage[1];

int main(void);
void aerortos_m10_hw_test_complete_marker(void);

static void check(bool condition)
{
    if (condition) {
        ++aerortos_m10_hw_pass_count;
    } else {
        ++aerortos_m10_hw_fail_count;
    }
}

static void block_or_panic(uint32_t code)
{
    if (aerortos_scheduler_block_current() != AERORTOS_SCHEDULER_OK) {
        ++aerortos_m10_hw_fail_count;
        platform_panic(code);
    }
}

static bool participant_complete(void)
{
    aerortos_critical_state_t state = aerortos_critical_enter();
    bool last;

    ++aerortos_m10_hw_participant_count;
    last = aerortos_m10_hw_participant_count == M10_PARTICIPANT_COUNT;
    if (last && (aerortos_scheduler_current() != aerortos_m10_high_task)) {
        if (aerortos_scheduler_make_ready(aerortos_m10_high_task) !=
            AERORTOS_SCHEDULER_OK) {
            ++aerortos_m10_hw_fail_count;
            aerortos_critical_exit(state);
            platform_panic(UINT32_C(0x4D31000A));
        }
    }
    aerortos_critical_exit(state);
    return last;
}

void aerortos_m10_hw_test_complete_marker(void)
{
    __asm volatile ("" ::: "memory");
}

static void idle_entry(void *argument)
{
    (void)argument;
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void peer_entry(void *argument)
{
    volatile uint32_t *complete = (volatile uint32_t *)argument;
    uint32_t index;

    for (index = 0U; index < 8U; ++index) {
        ++aerortos_m10_hw_voluntary_yield_count;
        check(aerortos_scheduler_yield() == AERORTOS_SCHEDULER_OK);
    }
    *complete = 1U;
    (void)participant_complete();
    block_or_panic(UINT32_C(0x4D310006));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void consumer_entry(void *argument)
{
    uint32_t value = 0U;

    (void)argument;
    check(aerortos_queue_receive(&aerortos_m10_queue, &value) ==
          AERORTOS_QUEUE_OK);
    ++aerortos_m10_hw_queue_operation_count;
    check(value == UINT32_C(0x4D310055));
    aerortos_m10_consumer_complete = 1U;
    (void)participant_complete();
    block_or_panic(UINT32_C(0x4D310007));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void periodic_entry(void *argument)
{
    uint32_t jobs = 0U;

    (void)argument;
    for (;;) {
        ++jobs;
        ++aerortos_m10_hw_periodic_job_count;
        if (jobs == 1U) {
            while (!aerortos_tick_after(
                aerortos_tick_now(),
                aerortos_m10_periodic_task->absolute_deadline)) {
                aerortos_wait_for_interrupt();
            }
        }
        if (jobs == M10_PERIODIC_SAMPLES) {
            aerortos_m10_periodic_complete = 1U;
            (void)participant_complete();
            block_or_panic(UINT32_C(0x4D310008));
        }
        check(aerortos_periodic_wait_next() == AERORTOS_TIMER_OK);
    }
}

static void high_entry(void *argument)
{
    const aerortos_measurement_series_t *critical_series =
        &aerortos_monitor_data.series[
            AERORTOS_METRIC_CRITICAL_SECTION_CYCLES];
    size_t queue_count = 1U;
    uint32_t index;

    (void)argument;
    block_or_panic(UINT32_C(0x4D310003));
    check(aerortos_m10_mutex.owner == aerortos_m10_low_task);
    check(aerortos_mutex_lock(&aerortos_m10_mutex) == AERORTOS_MUTEX_OK);
    ++aerortos_m10_hw_mutex_handoff_count;
    check(aerortos_mutex_unlock(&aerortos_m10_mutex) == AERORTOS_MUTEX_OK);
    aerortos_m10_high_complete = 1U;
    if (!participant_complete()) {
        block_or_panic(UINT32_C(0x4D310004));
    }

    check(aerortos_m10_hw_participant_count == M10_PARTICIPANT_COUNT);
    check((aerortos_m10_high_complete == 1U) &&
          (aerortos_m10_consumer_complete == 1U) &&
          (aerortos_m10_peer_a_complete == 1U) &&
          (aerortos_m10_peer_b_complete == 1U) &&
          (aerortos_m10_low_complete == 1U) &&
          (aerortos_m10_periodic_complete == 1U));
    check(aerortos_m10_hw_voluntary_yield_count == 16U);
    check(aerortos_m10_hw_queue_operation_count == 2U);
    check(aerortos_m10_hw_mutex_handoff_count == 1U);
    check(aerortos_m10_hw_periodic_job_count == M10_PERIODIC_SAMPLES);
    check(aerortos_monitor_data.core_clock_hz == UINT32_C(16000000));
    check(aerortos_monitor_data.series[
        AERORTOS_METRIC_INSTRUMENTATION_OVERHEAD_CYCLES].observation_count ==
        M10_OVERHEAD_SAMPLES &&
        aerortos_monitor_data.series[
        AERORTOS_METRIC_INSTRUMENTATION_OVERHEAD_CYCLES].retained_count ==
        M10_OVERHEAD_SAMPLES);
    check(aerortos_monitor_data.instrumentation_overhead_cycles != UINT32_MAX);
    check(aerortos_monitor_data.series[
        AERORTOS_METRIC_PENDSV_CYCLES].observation_count >= 16U);
    check(aerortos_monitor_data.series[
        AERORTOS_METRIC_DISPATCH_CYCLES].observation_count >= 16U);
    check(aerortos_monitor_data.series[
        AERORTOS_METRIC_PERIODIC_RELEASE_TO_START_CYCLES].observation_count >=
        M10_PERIODIC_SAMPLES);
    check(aerortos_monitor_data.series[
        AERORTOS_METRIC_SYNC_WAKE_CYCLES].observation_count >= 2U);
    check(critical_series->observation_count > AERORTOS_MEASUREMENT_CAPACITY &&
          critical_series->retained_count == AERORTOS_MEASUREMENT_CAPACITY &&
          critical_series->dropped_count ==
              critical_series->observation_count -
                  critical_series->retained_count &&
          critical_series->total_cycles != 0U &&
          critical_series->total_cycles >=
              (uint64_t)critical_series->minimum_cycles *
                  critical_series->observation_count &&
          critical_series->total_cycles <=
              (uint64_t)critical_series->maximum_cycles *
                  critical_series->observation_count &&
          critical_series->saturation_flags == 0U);
    check(aerortos_monitor_data.periodic_count >= M10_PERIODIC_SAMPLES);
    check(aerortos_monitor_data.deadline_miss_total == 1U);
    for (index = 0U; index < AERORTOS_METRIC_COUNT; ++index) {
        check(aerortos_monitor_data.series[index].observation_count != 0U);
    }
    check(aerortos_queue_count(&aerortos_m10_queue, &queue_count) ==
          AERORTOS_QUEUE_OK && queue_count == 0U);
    check((aerortos_m10_queue.waiting_senders == 0U) &&
          (aerortos_m10_queue.waiting_receivers == 0U));
    check(aerortos_m10_mutex.owner == NULL &&
          aerortos_m10_mutex.waiter_count == 0U);
    check(aerortos_queue_invariants_hold(&aerortos_m10_queue));
    check(aerortos_mutex_invariants_hold(&aerortos_m10_mutex));
    check(aerortos_scheduler_invariants_hold());
    check(aerortos_monitor_invariants_hold());
    check(aerortos_task_stack_guard_intact(aerortos_m10_high_task));
    check(aerortos_task_stack_guard_intact(aerortos_m10_periodic_task));
    check(aerortos_task_stack_guard_intact(aerortos_m10_consumer_task));
    check(aerortos_task_stack_guard_intact(aerortos_m10_peer_a_task));
    check(aerortos_task_stack_guard_intact(aerortos_m10_peer_b_task));
    check(aerortos_task_stack_guard_intact(aerortos_m10_low_task));

    if ((aerortos_m10_hw_pass_count == M10_EXPECTED_PASS_COUNT) &&
        (aerortos_m10_hw_fail_count == 0U) &&
        (aerortos_m10_hw_participant_count == M10_PARTICIPANT_COUNT)) {
        aerortos_m10_hw_complete = M10_COMPLETE_MAGIC;
    }
    aerortos_m10_hw_test_complete_marker();
    block_or_panic(UINT32_C(0x4D310005));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static void low_entry(void *argument)
{
    const uint32_t value = UINT32_C(0x4D310055);

    (void)argument;
    check(aerortos_mutex_lock(&aerortos_m10_mutex) == AERORTOS_MUTEX_OK);
    check(aerortos_scheduler_make_ready(aerortos_m10_high_task) ==
          AERORTOS_SCHEDULER_OK);
    while (aerortos_m10_mutex.waiter_count == 0U) {
        aerortos_wait_for_interrupt();
    }
    check(aerortos_m10_low_task->effective_priority ==
          aerortos_m10_high_task->base_priority);
    check(aerortos_queue_send(&aerortos_m10_queue, &value) ==
          AERORTOS_QUEUE_OK);
    ++aerortos_m10_hw_queue_operation_count;
    check(aerortos_mutex_unlock(&aerortos_m10_mutex) == AERORTOS_MUTEX_OK);
    check(aerortos_m10_low_task->effective_priority ==
          aerortos_m10_low_task->base_priority);
    aerortos_m10_low_complete = 1U;
    (void)participant_complete();
    block_or_panic(UINT32_C(0x4D310009));
    for (;;) {
        aerortos_wait_for_interrupt();
    }
}

static aerortos_task_t *create_task(
    aerortos_task_entry_t entry,
    void *argument,
    uint32_t *stack,
    uint8_t priority,
    uint32_t period,
    uint32_t deadline,
    uint32_t initial_delay)
{
    const aerortos_task_config_t config = {
        .entry = entry,
        .argument = argument,
        .stack = stack,
        .stack_size_bytes = M10_STACK_WORDS * sizeof(uint32_t),
        .priority = priority,
        .period_ticks = period,
        .relative_deadline_ticks = deadline,
        .initial_release_delay_ticks = initial_delay
    };
    aerortos_task_t *task = NULL;

    if (aerortos_task_create(&config, &task) != AERORTOS_TASK_OK) {
        ++aerortos_m10_hw_fail_count;
        platform_panic(UINT32_C(0x4D310001));
    }
    return task;
}

int main(void)
{
    if (!system_clock_init() || (SystemCoreClock != UINT32_C(16000000))) {
        platform_panic(UINT32_C(0x4D310002));
    }
    aerortos_task_system_init();
    aerortos_time_system_init(0U);
    check(aerortos_monitor_init(UINT32_C(16000000)));
    check(aerortos_monitor_calibrate_overhead(M10_OVERHEAD_SAMPLES));
    check(aerortos_queue_init(&aerortos_m10_queue, queue_storage, 1U,
                              sizeof(queue_storage[0])) == AERORTOS_QUEUE_OK);
    check(aerortos_mutex_init(&aerortos_m10_mutex) == AERORTOS_MUTEX_OK);
    aerortos_m10_idle_task = create_task(
        idle_entry, NULL, idle_stack, AERORTOS_IDLE_PRIORITY, 0U, 0U, 0U);
    aerortos_m10_high_task = create_task(
        high_entry, NULL, high_stack, 2U, 0U, 0U, 0U);
    aerortos_m10_periodic_task = create_task(
        periodic_entry, NULL, periodic_stack, 3U, 6U, 1U, 2U);
    aerortos_m10_consumer_task = create_task(
        consumer_entry, NULL, consumer_stack, 4U, 0U, 0U, 0U);
    aerortos_m10_peer_a_task = create_task(
        peer_entry, (void *)&aerortos_m10_peer_a_complete,
        peer_a_stack, 6U, 0U, 0U, 0U);
    aerortos_m10_peer_b_task = create_task(
        peer_entry, (void *)&aerortos_m10_peer_b_complete,
        peer_b_stack, 6U, 0U, 0U, 0U);
    aerortos_m10_low_task = create_task(
        low_entry, NULL, low_stack, 10U, 0U, 0U, 0U);
    aerortos_scheduler_init(aerortos_m10_idle_task);
    check(aerortos_scheduler_add(aerortos_m10_low_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m10_peer_a_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m10_peer_b_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m10_consumer_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m10_periodic_task) ==
          AERORTOS_SCHEDULER_OK);
    check(aerortos_scheduler_add(aerortos_m10_high_task) ==
          AERORTOS_SCHEDULER_OK);
    aerortos_port_init();
    check(aerortos_systick_init());
    aerortos_scheduler_start();
}
