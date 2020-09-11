#pragma once

#include <cstddef>
#include <exception>
#include <mimosa/future.hh>
#include <mimosa/ref-countable.hh>
#include <mimosa/shared-mutex.hh>
#include <mimosa/thread.hh>
#include <mimosa/trie.hxx>

#include "announce-request.hh"
#include "announce-response.hh"
#include "scrape-request.hh"
#include "scrape-response.hh"
#include "torrent.hh"

namespace hefur {
   class StatHandler;
   class PeersHandler;
   class FsTreeWhiteList;

   /**
    * This is the "in memory" torrent database.
    * Every public functions are thread safe.
    * The db is ref-counted because as we spawn threads, they may use the
    * db event after Hefur::stop().
    *
    * @internal as private data requires correct locking of torrents_lock_,
    * and in Hefur I do it by using RAII, there is no public interface to
    * look up the database, as it would involve to exposes a locking interface
    * which I don't want. I chose to use friend class when I need to do
    * specific queries in the db. Friend must classes *KNOW WHAT THEY DO*.
    *
    * The locking granularity is not good as we use a single shared mutex. So
    * when Hefur gets used by a lot of people, we just have to improve
    * the trie to have a better lock granularity.
    */
   class TorrentDb : public m::RefCountable<TorrentDb>, private m::NonCopyable {
   public:
      TorrentDb();
      ~TorrentDb();

      /**
       * Execute the request and store the response in the return value.
       * @return A valid response, but you have to check for null pointer
       * in case of weird internal error.
       *
       * @{
       */
      AnnounceResponse::Ptr announce(AnnounceRequest::Ptr request);
      ScrapeResponse::Ptr scrape(ScrapeRequest::Ptr request);
      /** @} */

      /**
       * Adds a torrent to torrents_. If the torrent is already
       * present, it will just drop the pointer.
       */
      void addTorrent(Torrent::Ptr torrent);

      /**
       * Removes a torrent by its info hash.
       */
      void removeTorrent(const m::StringRef &info_hash);

   private:
      friend class StatHandler;
      friend class PeersHandler;
      friend class FileHandler;
      friend class FsTreeWhiteList;

      struct TorrentEntry {
         TorrentEntry(Torrent::Ptr t = nullptr, int v = 1) : torrent(t), version(v) {}
         TorrentEntry(std::nullptr_t) {}
         operator bool() const noexcept { return torrent; }
         Torrent& operator->() const noexcept { return *torrent; }

         Torrent::Ptr torrent;
         int version = 0;
      };

      /** helper to use torrent->key() as a key for the trie */
      static inline m::StringRef torrentKey(TorrentEntry torrent) {
         switch (torrent.version) {
         case 1:
            return torrent.torrent->keyV1();
         case 2:
            return torrent.torrent->keyV2().substr(0, 20);
         default:
            std::terminate();
         }
      }

      typedef m::Trie<TorrentEntry, torrentKey> torrents_type;

      void cleanup();
      void cleanupLoop();

      m::Future<bool> cleanup_stop_;
      m::Thread cleanup_thread_;
      m::SharedMutex torrents_lock_;
      torrents_type torrents_;
   };
} // namespace hefur
