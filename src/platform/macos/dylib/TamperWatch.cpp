#include "stdafx.hpp"

#include "TamperWatch.hpp"
#include "Image.hpp"

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>

namespace
{
// state words of AntiTamper_StartupTextHash_55, _199, _227, _236 (Apple silicon has 4 watchpoint registers)
constexpr uint64_t Watched[] = {0x105019000, 0x105060d20, 0x105065460, 0x105065fb0};
// instruction breakpoints: the branch unit_size_patch rewrites (sub_101a516d8, x20 = entry index; the patch only
// changes behaviour for indexes >= 64), to see whether that code runs when the quirks appear
constexpr uint64_t Breakpoints[] = {0x101a51c58};

mach_port_t exceptionPort = MACH_PORT_NULL;
thread_act_t serverThread = MACH_PORT_NULL;

#pragma pack(push, 4)
struct ExceptionRequest
{
    mach_msg_header_t head;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t thread;
    mach_msg_port_descriptor_t task;
    NDR_record_t ndr;
    exception_type_t exception;
    mach_msg_type_number_t codeCount;
    int64_t code[2];
    mach_msg_trailer_t trailer;
};

struct ExceptionReply
{
    mach_msg_header_t head;
    NDR_record_t ndr;
    kern_return_t result;
};
#pragma pack(pop)

arm_debug_state64_t MakeDebugState()
{
    arm_debug_state64_t state{};
    for (size_t i = 0; i < std::size(Watched); i++)
    {
        const auto address = Image::Get()->Resolve(Watched[i]);
        // 4 bytes at an 8-byte aligned address: byte address select 0x0f; EL0, loads and stores, enabled
        const uint64_t byteSelect = 0x0Full << (address & 7);
        state.__wvr[i] = address & ~7ull;
        state.__wcr[i] = 1 | (2 << 1) | (3 << 3) | (byteSelect << 5);
    }
    for (size_t i = 0; i < std::size(Breakpoints); i++)
    {
        // A64 instruction (byte address select 0xf), EL0, enabled
        state.__bvr[i] = Image::Get()->Resolve(Breakpoints[i]);
        state.__bcr[i] = 1 | (2 << 1) | (0xF << 5);
    }
    return state;
}

void* ServeExceptions(void*)
{
    pthread_setname_np("TWASE tamper watch");

    while (true)
    {
        ExceptionRequest request{};
        if (mach_msg(&request.head, MACH_RCV_MSG, 0, sizeof(request), exceptionPort, MACH_MSG_TIMEOUT_NONE,
                     MACH_PORT_NULL) != KERN_SUCCESS)
        {
            continue;
        }

        const thread_act_t thread = request.thread.name;

        arm_thread_state64_t threadState{};
        mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
        uint64_t pc = 0;
        uint64_t lr = 0;
        uint64_t x20 = 0;
        std::string extra;
        if (thread_get_state(thread, ARM_THREAD_STATE64, reinterpret_cast<thread_state_t>(&threadState), &count) ==
            KERN_SUCCESS)
        {
            pc = arm_thread_state64_get_pc(threadState);
            lr = arm_thread_state64_get_lr(threadState);
            x20 = threadState.__x[20];

            // the unit_size_patch loop: x19 = current list node (next at +8), x25 = end of the list, mask at sp + 0x28
            if (pc == Image::Get()->Resolve(Breakpoints[0]))
            {
                const auto readU64 = [](uint64_t aAddress, uint64_t& aValue)
                {
                    mach_vm_size_t size = 0;
                    return mach_vm_read_overwrite(mach_task_self(), aAddress, sizeof(aValue),
                                                  reinterpret_cast<mach_vm_address_t>(&aValue), &size) == KERN_SUCCESS;
                };
                uint64_t node = threadState.__x[19];
                const uint64_t end = threadState.__x[25];
                size_t length = 0;
                while (node != end && length < 100000 && readU64(node + 8, node))
                {
                    length++;
                }
                uint64_t mask = 0;
                readU64(arm_thread_state64_get_sp(threadState) + 0x28, mask);
                extra = fmt::format(", list length {}{}, mask {:#x}", length, length >= 64 ? " (PATCHED PATH)" : "", mask);
            }
        }

        uint64_t threadId = 0;
        thread_identifier_info_data_t idInfo{};
        mach_msg_type_number_t idCount = THREAD_IDENTIFIER_INFO_COUNT;
        if (thread_info(thread, THREAD_IDENTIFIER_INFO, reinterpret_cast<thread_info_t>(&idInfo), &idCount) ==
            KERN_SUCCESS)
        {
            threadId = idInfo.thread_id;
        }

        const auto slide = Image::Get()->GetSlide();
        spdlog::info("[TamperWatch] exception {} code {:#x} address {:#x} at pc {:#x} (lr {:#x}, x20 {:#x}), thread {}{}",
                     request.exception, request.code[0], request.code[1] - slide, pc - slide, lr - slide, x20,
                     threadId, extra);

        // no more hits on this thread until the next Arm(), the access then completes normally
        arm_debug_state64_t cleared{};
        thread_set_state(thread, ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&cleared),
                         ARM_DEBUG_STATE64_COUNT);

        mach_port_deallocate(mach_task_self(), request.thread.name);
        mach_port_deallocate(mach_task_self(), request.task.name);

        ExceptionReply reply{};
        reply.head.msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request.head.msgh_bits), 0);
        reply.head.msgh_remote_port = request.head.msgh_remote_port;
        reply.head.msgh_size = sizeof(reply);
        reply.head.msgh_id = request.head.msgh_id + 100;
        reply.ndr = NDR_record;
        reply.result = KERN_SUCCESS;
        mach_msg(&reply.head, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    }
    return nullptr;
}

bool StartServer()
{
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exceptionPort) != KERN_SUCCESS ||
        mach_port_insert_right(mach_task_self(), exceptionPort, exceptionPort, MACH_MSG_TYPE_MAKE_SEND) !=
            KERN_SUCCESS)
    {
        spdlog::error("[TamperWatch] could not create the exception port");
        return false;
    }

    pthread_t thread;
    if (pthread_create(&thread, nullptr, ServeExceptions, nullptr) != 0)
    {
        spdlog::error("[TamperWatch] could not start the exception thread");
        return false;
    }
    pthread_detach(thread);
    serverThread = pthread_mach_thread_np(thread);

    spdlog::info("[TamperWatch] watching {:#x}, {:#x}, {:#x}, {:#x}", Watched[0], Watched[1], Watched[2], Watched[3]);
    return true;
}
} // namespace

void TamperWatch::Arm()
{
    if (!Image::Get()->IsSupported())
    {
        return;
    }

    static const bool started = StartServer();
    if (!started)
    {
        return;
    }

    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t threadCount = 0;
    if (task_threads(mach_task_self(), &threads, &threadCount) != KERN_SUCCESS)
    {
        return;
    }

    auto state = MakeDebugState();
    size_t armed = 0;
    for (mach_msg_type_number_t i = 0; i < threadCount; i++)
    {
        if (threads[i] != serverThread &&
            thread_set_exception_ports(threads[i], EXC_MASK_BREAKPOINT, exceptionPort,
                                       EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, ARM_THREAD_STATE64) == KERN_SUCCESS &&
            thread_set_state(threads[i], ARM_DEBUG_STATE64, reinterpret_cast<thread_state_t>(&state),
                             ARM_DEBUG_STATE64_COUNT) == KERN_SUCCESS)
        {
            armed++;
        }
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), threadCount * sizeof(thread_act_t));

    static size_t lastArmed = 0;
    if (armed != lastArmed)
    {
        spdlog::debug("[TamperWatch] armed {} of {} threads", armed, threadCount);
        lastArmed = armed;
    }

    // self-test: this read on the current thread must show up as a hit (pc inside libTWASE)
    static bool tested = false;
    if (!tested)
    {
        tested = true;
        const auto value = *Image::Get()->Resolve<const volatile uint32_t>(Watched[0]);
        spdlog::info("[TamperWatch] self-test read of {:#x}: {:#x}", Watched[0], value);
    }
}
