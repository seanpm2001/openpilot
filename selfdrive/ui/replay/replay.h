#pragma once

#include <optional>

#include <QThread>

#include "selfdrive/ui/replay/camera.h"
#include "selfdrive/ui/replay/route.h"

// one segment uses about 100M of memory
constexpr int FORWARD_SEGS = 5;

enum REPLAY_FLAGS {
  REPLAY_FLAG_NONE = 0x0000,
  REPLAY_FLAG_DCAM = 0x0002,
  REPLAY_FLAG_ECAM = 0x0004,
  REPLAY_FLAG_NO_LOOP = 0x0010,
  REPLAY_FLAG_NO_FILE_CACHE = 0x0020,
  REPLAY_FLAG_QCAMERA = 0x0040,
  REPLAY_FLAG_SEND_YUV = 0x0080,
  REPLAY_FLAG_NO_CUDA = 0x0100,
  REPLAY_FLAG_FULL_SPEED = 0x0200,
  REPLAY_FLAG_NO_VIPC = 0x0400,
};

enum class FindFlag {
  nextEngagement,
  nextDisEngagement
};

class Replay : public QObject {
  Q_OBJECT

public:
  Replay(QString route, QStringList allow, QStringList block, SubMaster *sm = nullptr,
          uint32_t flags = REPLAY_FLAG_NONE, QString data_dir = "", QObject *parent = 0);
  ~Replay();
  bool load();
  void start(int seconds = 0);
  void pause(bool pause);
  bool isPaused() const { return paused_; }
  inline bool hasFlag(REPLAY_FLAGS flag) const { return flags_ & flag; }
  inline void addFlag(REPLAY_FLAGS flag) { flags_ |= flag; }
  inline void removeFlag(REPLAY_FLAGS flag) { flags_ &= ~flag; }

signals:
  void segmentChanged();
  void seekTo(int seconds, bool relative);
  void seekToFlag(FindFlag flag);

protected slots:
  void queueSegment();
  void doSeek(int seconds, bool relative);
  void doSeekToFlag(FindFlag flag);
  void segmentLoadFinished(bool sucess);

protected:
  typedef std::map<int, std::unique_ptr<Segment>> SegmentMap;
  std::optional<uint64_t> find(FindFlag flag);
  void startStream(const Segment *cur_segment);
  void stream();
  void setCurrentSegment(int n);
  void mergeSegments(const SegmentMap::iterator &begin, const SegmentMap::iterator &end);
  void updateEvents(const std::function<bool()>& lambda);
  void publishMessage(const Event *e);
  void publishFrame(const Event *e);
  inline int currentSeconds() const { return (cur_mono_time_ - route_start_ts_) / 1e9; }
  inline bool isSegmentMerged(int n) {
    return std::find(segments_merged_.begin(), segments_merged_.end(), n) != segments_merged_.end();
  }

  QThread *stream_thread_ = nullptr;

  // logs
  std::mutex stream_lock_;
  std::condition_variable stream_cv_;
  std::atomic<bool> updating_events_ = false;
  std::atomic<int> current_segment_ = 0;
  SegmentMap segments_;
  // the following variables must be protected with stream_lock_
  bool exit_ = false;
  bool paused_ = false;
  bool events_updated_ = false;
  uint64_t route_start_ts_ = 0;
  uint64_t cur_mono_time_ = 0;
  std::unique_ptr<std::vector<Event *>> events_;
  std::unique_ptr<std::vector<Event *>> new_events_;
  std::vector<int> segments_merged_;

  // messaging
  SubMaster *sm = nullptr;
  std::unique_ptr<PubMaster> pm;
  std::vector<const char*> sockets_;
  std::unique_ptr<Route> route_;
  std::unique_ptr<CameraServer> camera_server_;
  std::atomic<uint32_t> flags_ = REPLAY_FLAG_NONE;
};
