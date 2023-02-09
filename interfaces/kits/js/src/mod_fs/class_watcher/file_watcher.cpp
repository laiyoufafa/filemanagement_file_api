/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "file_watcher.h"
#include <unistd.h>
#include <errno.h>
#include "filemgmt_libhilog.h"
#include "uv.h"
namespace OHOS::FileManagement::ModuleFileIO {
using namespace OHOS::FileManagement::LibN; 
const int BUF_SIZE = 1024;
FileWatcher::FileWatcher() {}

FileWatcher::~FileWatcher() {}

bool FileWatcher::InitNotify(int &fd)
{
    fd = inotify_init();
    if (fd == -1) {
        HILOGE("FileWatcher InitNotify fail errCode:%{public}d", errno);
        return false;
    }
    return true;
}

bool FileWatcher::StartNotify(std::shared_ptr<WatcherInfoArg> &arg)
{
    int wd = 0;
    uint32_t watchEvent = 0;
    for (auto event : arg->events) {
        watchEvent = watchEvent | event;
    }
    wd = inotify_add_watch(arg->fd, arg->filename.c_str(), watchEvent);
    if(wd == -1) {
        HILOGE("FileWatcher StartNotify fail errCode:%{public}d", errno);
        return false;
    }
    arg->wd = wd;
    run_ = true;
    return true;
}

bool FileWatcher::StopNotify(std::shared_ptr<WatcherInfoArg> &arg)
{
    run_ = false;
    if (inotify_rm_watch(arg->fd, arg->wd) == -1) {
        HILOGE("FileWatcher StopNotify fail errCode:%{public}d", errno);
        return false;
    }
    close(arg->fd);
    return true;
}

void FileWatcher::HandleEvent(std::shared_ptr<WatcherInfoArg> &arg,
                              const struct inotify_event *event,
                              WatcherCallback callback)
{
    if (event->wd == arg->wd) {
        for (auto watchEvent : arg->events) {
            if (event->mask == watchEvent || watchEvent == IN_ALL_EVENTS) {
                std::string filename = arg->filename + "/" + event->name;
                HILOGI("FileWatcher HandleEvent fileName:%{public}s, mask:%{public}d", filename.c_str(), event->mask);
                callback(arg->env, arg->ref, filename, event->mask);
            }
        }
    }
}

void FileWatcher::GetNotifyEvent(std::shared_ptr<WatcherInfoArg> &arg, WatcherCallback callback)
{
    char buf[BUF_SIZE] = {0};
    struct inotify_event *event = nullptr;
    while (run_) {
        int fd = arg->fd;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        if (select(fd + 1, &fds, NULL, NULL, NULL) > 0) {
            int len, index = 0;
            while (((len = read(fd, &buf, sizeof(buf))) < 0) && (errno == EINTR));
			while (index < len) {
				event = (struct inotify_event *)(buf + index);
				HandleEvent(arg, event, callback);
				index += sizeof(struct inotify_event) + event->len;
			}
        }
    }
}
} // namespace OHOS::FileManagement::ModuleFileIO