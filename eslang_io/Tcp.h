#include <boost/intrusive_ptr.hpp>
#include <eslang/BaseTypes.h>
#include <eslang/Context.h>
#include <numeric>

#include <boost/asio/ssl/context.hpp>

namespace s {

class Buffer {
public:
  unsigned char* data() const { return buffer_->data() + offset_; }
  size_t size() const { return length_; }
  boost::asio::const_buffer range() const {
    return boost::asio::const_buffer{data(), size()};
  }
  static Buffer makeCopy(void const* data, size_t len) {
    BuffIP b(new Buff(data, len));
    return Buffer(b);
  }
  static Buffer makeCopy(std::string const& s) {
    return makeCopy(s.data(), s.size());
  }
  static Buffer make(std::vector<unsigned char> data) {
    BuffIP b(new Buff(std::move(data)));
    return Buffer(b);
  }
  void consume(size_t n) {
    assert(length_ >= n);
    length_ -= n;
    offset_ += n;
  }
  void append(Buffer const& b) {
    buffer_->append(b.data(), b.size());
    length_ += b.size();
  }
  Buffer() = delete;

private:
  class Buff {
  public:
    unsigned char const* data() const { return data_.data(); }
    unsigned char* data() { return data_.data(); }
    size_t size() const { return data_.size(); }
    size_t capacity() const { return data_.capacity(); }
    void append(void* data, size_t len) {}
    void reserve(size_t n) { data_.reserve(n); }
    friend void intrusive_ptr_add_ref(Buff* p) { ++p->refs_; }
    friend void intrusive_ptr_release(Buff* p) {
      if (--p->refs_ == 0)
        delete p;
    }
    Buff(void const* data, size_t len)
        : data_(static_cast<unsigned char const*>(data),
                static_cast<unsigned char const*>(data) + len) {}
    Buff(std::vector<unsigned char> data) : data_(std::move(data)) {}

  private:
    std::vector<unsigned char> data_;
    uint32_t refs_ = 0;
  };
  using BuffIP = boost::intrusive_ptr<Buff>;
  Buffer(BuffIP b) : buffer_(std::move(b)), length_(buffer_->size()) {}
  BuffIP buffer_;
  size_t offset_ = 0;
  size_t length_ = 0;
};

struct BufferCollection {
  std::vector<Buffer> buffers;
  Buffer combine() const {
    size_t len = std::accumulate(
        buffers.begin(), buffers.end(), size_t(0),
        [](size_t acc, Buffer const& b) -> size_t { return acc + b.size(); });
    std::vector<unsigned char> v;
    v.reserve(len);
    std::for_each(buffers.begin(), buffers.end(), [&](Buffer const& b) {
      v.insert(v.end(), b.data(), b.data() + b.size());
    });
    return Buffer::make(std::move(v));
  }
};

class Tcp {
public:
  struct SocketOptions {
    bool throttled = true;
  };

  struct ListenerOptions : SocketOptions {
    explicit ListenerOptions(uint32_t port) : port(port) {}
    uint32_t port;
    struct SslFiles {
      std::string ca;
      std::string cert;
      std::string key;
    };
    std::optional<std::function<std::unique_ptr<boost::asio::ssl::context>(
        boost::asio::io_service&)>>
        sslContextFactory;
    ListenerOptions withSslFiles(std::string ca, std::string cert,
                                 std::string key) const;
  };
  struct Socket {
    explicit Socket(Pid p) : pid(std::move(p)) {}
    Pid pid;
  };
  struct ReceiveData {
    explicit ReceiveData(Pid p, Buffer data)
        : sender(p), data(std::move(data)) {}
    Pid sender;
    Buffer data;
  };
  static Pid makeListener(Process* parent,
                          TSendAddress<Socket> new_socket_address,
                          ListenerOptions options);

  static void initRecvSocket(Process* sender, Socket socket,
                             TSendAddress<ReceiveData> new_socket_address);

  static MethodTask<> sendThrottled(Process* sender, Socket socket,
                                    Buffer data);
  static MethodTask<> sendManyThrottled(Process* sender, Socket socket,
                                        BufferCollection buffs);
  static WaitingMaybe send(Process* sender, Socket socket, Buffer data);
  static WaitingMaybe sendMany(Process* sender, Socket socket,
                               BufferCollection buffs);
};

struct StreamBatcher : NonMovable {
  Process* sender;
  Tcp::Socket socket;
  BufferCollection buffs;
  StreamBatcher(Process* sender, Tcp::Socket socket)
      : sender(sender), socket(std::move(socket)) {}
  void push(Buffer b) { buffs.buffers.push_back(std::move(b)); }

  void clear() {
    if (buffs.buffers.size()) {
      Tcp::sendMany(sender, socket, std::move(buffs));
    }
  }

  ~StreamBatcher() { clear(); }
};
}