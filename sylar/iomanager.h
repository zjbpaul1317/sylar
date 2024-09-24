/**
 * @file iomanager.h
 * @brief IO协程调度器
 */

#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace sylar
{
    class IOManager : public Scheduler, public TimerManager
    {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        /**
         * @brief IO事件，继承自epoll对事件的定义
         * @details 只关心soclet fd 的读和写事件，其他epoll事件会归类到这两类事件中
         */
        enum Event
        {
            // 无事件
            NONE = 0x0,

            // 读时间(EPOLLIN)
            READ = 0x1,

            // 写事件(EPOLLOUT)
            WRITE = 0x4,
        };

    private:
        /**
         * @brief socket fd 上下文类
         * @details 每个socket fd 都对应衣蛾FdContext，包括fd的值，fd上的事件，以及fd读写事件上下文
         */
        struct FdContext
        {
            typedef Mutex MutexType;

            /**
             * @brief 事件上下文类
             * @details fd的每个事件都有一个事件上下文，保存这个事件的回调函数以及执行回调函数的调度器
             *          sylar对fd事件做了简化，只预留了读事件和写事件，所有的事件都被归类到这两类事件中
             */
            struct EventContext
            {
                // 执行事件回调的调度器
                Scheduler *scheduler = nullptr;

                // 事件回调协程
                Fiber::ptr fiber;

                // 事件回调函数
                std::function<void()> cb;
            };

            /**
             * @brief 获取事件上下文类
             * @param[in] event 事件类型
             * @return 返回对应事件的上下文
             */
            EventContext &getEventContext(Event event);

            /**
             * @brief 重置事件上下文
             * @param[in, out] ctx 待重置的事件上下文对象
             */
            void resetEventContext(EventContext &ctx);

            /**
             * @brief 触发事件
             * @details 根据事件类型调用对应上下文结构中的调度器去调度回调协程或回调函数
             * @param[in] event 事件类型
             */
            void triggerEvent(Event event);

            // 读事件上下文
            EventContext read;
            // 写事件上下文
            EventContext write;
            // 事件关联的句柄
            int fd = 0;
            // 该fd添加了哪些事件的回调函数，或者说该fd关心了哪些事件
            Event events = NONE;
            // 事件的Mutex
            MutexType mutex;
        };

    public:
        /**
         * @brief 构造函数
         * @param[in] threads 线程数量
         * @param[in] use_caller 是否将调用线程包含进去
         * @param[in] name 调度器的名称
         */
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "IOManager");

        /**
         * @brief 析构函数
         */
        ~IOManager();

        /**
         * @brief 添加事件
         * @details fd描述符发生了event事件时执行cb函数
         * @param[in] fd socket句柄
         * @param[in] event 事件类型
         * @param[in] cb 事件回调函数，如果为空，则默认把当前协程作为回调执行体
         * @return 添加成功返回0，失败返回-1
         */
        int addEvent(int fd, Event event, std::function<void()> cb = nullptr);

        /**
         * @brief 删除事件
         * @param[in] fd socket句柄
         * @param[in] event事件类型
         * @attention 不会触发事件
         * @return 是否删除成功
         */
        bool delEvent(int fd, Event event);

        bool cancelEvent(int fd, Event event);

        bool cancelAll(int fd);

        static IOManager *GetThis();

    protected:
        void tickle() override;

        bool stopping() override;

        void idle() override;

        bool stopping(unit64_t &timeout);

        void onTimerInsertedAtFront() override;

        void contextResize(size_t size);

    private:
        int m_epfd = 0;

        int m_tickleFds[2];

        std::atomic<size_t> m_pendingEventCount = {0};

        RWMutexType m_mutex;

        std::vector<FdContext *> m_fdContexts;
    };
}

#endif