#include "MyMesh.h"
#include <algorithm>

/* ------------------------------ Config -------------------------------- */

#ifndef LORA_FREQ
  #define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
  #define LORA_BW 250
#endif
#ifndef LORA_SF
  #define LORA_SF 10
#endif
#ifndef LORA_CR
  #define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
  #define LORA_TX_POWER 20
#endif

#ifndef ADVERT_NAME
  #define ADVERT_NAME "repeater"
#endif
#ifndef ADVERT_LAT
  #define ADVERT_LAT 0.0
#endif
#ifndef ADVERT_LON
  #define ADVERT_LON 0.0
#endif

#ifndef ADMIN_PASSWORD
  #define ADMIN_PASSWORD "password"
#endif

#ifndef SERVER_RESPONSE_DELAY
  #define SERVER_RESPONSE_DELAY 300
#endif

#ifndef TXT_ACK_DELAY
  #define TXT_ACK_DELAY 200
#endif

#define FIRMWARE_VER_LEVEL       2

#define REQ_TYPE_GET_STATUS         0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE         0x02
#define REQ_TYPE_GET_TELEMETRY_DATA 0x03
#define REQ_TYPE_GET_ACCESS_LIST    0x05
#define REQ_TYPE_GET_NEIGHBOURS     0x06
#define REQ_TYPE_GET_OWNER_INFO     0x07     // FIRMWARE_VER_LEVEL >= 2

#define RESP_SERVER_LOGIN_OK        0 // response to ANON_REQ

#define ANON_REQ_TYPE_REGIONS      0x01
#define ANON_REQ_TYPE_OWNER        0x02
#define ANON_REQ_TYPE_BASIC        0x03   // just remote clock

#define CLI_REPLY_DELAY_MILLIS      600

#define LAZY_CONTACTS_WRITE_DELAY    5000

#ifdef LOON_FIRMWARE
#define LOON_PREFS_MAGIC             0x4C4F4F4EUL
#define LOON_PREFS_VERSION           3
#define LOON_PREFS_FILE              "/loon_prefs"
#define LOON_ANNOUNCE_OFF            0
#define LOON_ANNOUNCE_HOURLY         1
#define LOON_ANNOUNCE_DAILY          2
#define LOON_VALID_CLOCK             1700000000UL
#define LOON_COMMAND_COOLDOWN_MS     10000UL
#define LOON_COMMAND_SENDER_SLOTS    8
#define LOON_MAX_ANNOUNCEMENT_TEXT   (MAX_PACKET_PAYLOAD - CIPHER_BLOCK_SIZE - 5)
#if defined(ESP32)
#define LOON_WEBHOOK_PREFS_MAGIC      0x4C57484BUL
#define LOON_WEBHOOK_PREFS_VERSION    1
#define LOON_WEBHOOK_PREFS_FILE       "/loon_webhook"
#define LOON_WEBHOOK_MIN_INTERVAL_MS  200UL
#define LOON_WEBHOOK_RETRY_DELAY_MS   2000UL
#define LOON_WIFI_RETRY_DELAY_MS      15000UL
#endif

static const uint8_t LOON_PUBLIC_SECRET[16] = {
  0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
  0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72
};
static const uint8_t LOON_TEST_SECRET[16] = {
  0x9c, 0xd8, 0xfc, 0xf2, 0x2a, 0x47, 0x33, 0x3b,
  0x59, 0x1d, 0x96, 0xa2, 0xb8, 0x48, 0xb7, 0x3f
};

struct LoonPrefsV1 {
  uint32_t magic;
  uint8_t version;
  uint8_t ping_public;
  uint8_t ping_test;
  uint8_t announce_public;
  uint8_t announce_test;
  uint8_t daily_hour;
  int16_t timezone_minutes;
  uint8_t busy_threshold;
  uint16_t max_busy_delay_secs;
  uint32_t checksum;
};

struct LoonPrefsV2 {
  uint32_t magic;
  uint8_t version;
  uint8_t ping_public;
  uint8_t ping_test;
  uint8_t announce_public;
  uint8_t announce_test;
  uint8_t daily_hour;
  int16_t timezone_minutes;
  uint8_t busy_threshold;
  uint16_t max_busy_delay_secs;
  char announcement_message[141];
  uint32_t checksum;
};

static uint32_t calcLoonChecksum(const void* data, size_t len) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < len; i++) hash = (hash ^ p[i]) * 16777619UL;
  return hash;
}

static void formatLoonPath(const mesh::Packet* packet, char* dest, size_t dest_len) {
  if (!dest_len) return;
  dest[0] = 0;
  if (!packet || packet->getPathHashCount() == 0) {
    StrHelper::strncpy(dest, "direct", dest_len);
    return;
  }
  if (!mesh::Packet::isValidPathLen(packet->path_len)) {
    StrHelper::strncpy(dest, "invalid", dest_len);
    return;
  }
  uint8_t count = packet->getPathHashCount();
  uint8_t size = packet->getPathHashSize();
  size_t used = 0;
  for (uint8_t i = 0; i < count && used + 1 < dest_len; i++) {
    if (i) dest[used++] = '>';
    for (uint8_t j = 0; j < size && used + 2 < dest_len; j++) {
      int n = snprintf(dest + used, dest_len - used, "%02X", packet->path[i * size + j]);
      if (n != 2) { dest[dest_len - 1] = 0; return; }
      used += 2;
    }
  }
  dest[used] = 0;
}

static bool loonCommandIs(const char* body, const char* command) {
  while (*body == ' ') body++;
  size_t n = strlen(command);
  if (strncasecmp(body, command, n) != 0) return false;
  body += n;
  while (*body == ' ') body++;
  return *body == 0;
}

static bool loonAnnouncementIsSafe(const char* text) {
  while (*text == ' ') text++;
  return *text != '!';
}

static const char* loonModeName(uint8_t mode) {
  return mode == LOON_ANNOUNCE_HOURLY ? "hourly" : mode == LOON_ANNOUNCE_DAILY ? "daily" : "off";
}

#if defined(ESP32)
static void loonJsonEscape(const char* src, char* dest, size_t dest_len) {
  if (!dest_len) return;
  size_t used = 0;
  while (*src && used + 1 < dest_len) {
    const char* escaped = NULL;
    switch (*src) {
      case '\\': escaped = "\\\\"; break;
      case '"': escaped = "\\\""; break;
      case '\n': escaped = "\\n"; break;
      case '\r': escaped = "\\r"; break;
      case '\t': escaped = "\\t"; break;
      default: break;
    }
    if (escaped) {
      if (used + 2 >= dest_len) break;
      dest[used++] = escaped[0];
      dest[used++] = escaped[1];
    } else if ((uint8_t)*src >= 0x20) {
      dest[used++] = *src;
    }
    src++;
  }
  dest[used] = 0;
}
#endif
#endif

void MyMesh::putNeighbour(const mesh::Identity &id, uint32_t timestamp, float snr) {
#if MAX_NEIGHBOURS // check if neighbours enabled
  // find existing neighbour, else use least recently updated
  uint32_t oldest_timestamp = 0xFFFFFFFF;
  NeighbourInfo *neighbour = &neighbours[0];
  for (int i = 0; i < MAX_NEIGHBOURS; i++) {
    // if neighbour already known, we should update it
    if (id.matches(neighbours[i].id)) {
      neighbour = &neighbours[i];
      break;
    }

    // otherwise we should update the least recently updated neighbour
    if (neighbours[i].heard_timestamp < oldest_timestamp) {
      neighbour = &neighbours[i];
      oldest_timestamp = neighbour->heard_timestamp;
    }
  }

  // update neighbour info
  neighbour->id = id;
  neighbour->advert_timestamp = timestamp;
  neighbour->heard_timestamp = getRTCClock()->getCurrentTime();
  neighbour->snr = (int8_t)(snr * 4);
#endif
}

uint8_t MyMesh::handleLoginReq(const mesh::Identity& sender, const uint8_t* secret, uint32_t sender_timestamp, const uint8_t* data, bool is_flood) {
  ClientInfo* client = NULL;
  if (data[0] == 0) {   // blank password, just check if sender is in ACL
    client = acl.getClient(sender.pub_key, PUB_KEY_SIZE);
    if (client == NULL) {
    #if MESH_DEBUG
      MESH_DEBUG_PRINTLN("Login, sender not in ACL");
    #endif
    }
  }
  if (client == NULL) {
    uint8_t perms;
    if (strcmp((char *)data, _prefs.password) == 0) { // check for valid admin password
      perms = PERM_ACL_ADMIN;
    } else if (strcmp((char *)data, _prefs.guest_password) == 0) { // check guest password
      perms = PERM_ACL_GUEST;
    } else {
#if MESH_DEBUG
      MESH_DEBUG_PRINTLN("Invalid password: %s", data);
#endif
      return 0;
    }

    client = acl.putClient(sender, 0);  // add to contacts (if not already known)
    if (sender_timestamp <= client->last_timestamp) {
      MESH_DEBUG_PRINTLN("Possible login replay attack!");
      return 0;  // FATAL: client table is full -OR- replay attack
    }

    MESH_DEBUG_PRINTLN("Login success!");
    client->last_timestamp = sender_timestamp;
    client->last_activity = getRTCClock()->getCurrentTime();
    client->permissions &= ~0x03;
    client->permissions |= perms;
    memcpy(client->shared_secret, secret, PUB_KEY_SIZE);

    if (perms != PERM_ACL_GUEST) {   // keep number of FS writes to a minimum
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
    }
  }

  if (is_flood) {
    client->out_path_len = OUT_PATH_UNKNOWN;  // need to rediscover out_path
  }

  uint32_t now = getRTCClock()->getCurrentTimeUnique();
  memcpy(reply_data, &now, 4);   // response packets always prefixed with timestamp
  reply_data[4] = RESP_SERVER_LOGIN_OK;
  reply_data[5] = 0;  // Legacy: was recommended keep-alive interval (secs / 16)
  reply_data[6] = client->isAdmin() ? 1 : 0;
  reply_data[7] = client->permissions;
  getRNG()->random(&reply_data[8], 4);   // random blob to help packet-hash uniqueness
  reply_data[12] = FIRMWARE_VER_LEVEL;  // New field

  return 13;  // reply length
}

uint8_t MyMesh::handleAnonRegionsReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data) {
  if (anon_limiter.allow(rtc_clock.getCurrentTime())) {
    // request data has: {reply-path-len}{reply-path}
    reply_path_len = *data++;
    if (!mesh::Packet::isValidPathLen(reply_path_len)) return 0;  // reject - bad encoding

    mesh::Packet::writePath(reply_path, data, reply_path_len);
    // data += (uint8_t)reply_path_len * reply_path_hash_size;

    memcpy(reply_data, &sender_timestamp, 4);   // prefix with sender_timestamp, like a tag
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply_data[4], &now, 4);     // include our clock (for easy clock sync, and packet hash uniqueness)

    return 8 + region_map.exportNamesTo((char *) &reply_data[8], sizeof(reply_data) - 12, REGION_DENY_FLOOD);   // reply length
  }
  return 0;
}

uint8_t MyMesh::handleAnonOwnerReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data) {
  if (anon_limiter.allow(rtc_clock.getCurrentTime())) {
    // request data has: {reply-path-len}{reply-path}
    reply_path_len = *data++;
    if (!mesh::Packet::isValidPathLen(reply_path_len)) return 0;  // reject - bad encoding

    mesh::Packet::writePath(reply_path, data, reply_path_len);
    // data += (uint8_t)reply_path_len * reply_path_hash_size;

    memcpy(reply_data, &sender_timestamp, 4);   // prefix with sender_timestamp, like a tag
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply_data[4], &now, 4);     // include our clock (for easy clock sync, and packet hash uniqueness)
    sprintf((char *) &reply_data[8], "%s\n%s", _prefs.node_name, _prefs.owner_info);

    return 8 + strlen((char *) &reply_data[8]);   // reply length
  }
  return 0;
}

uint8_t MyMesh::handleAnonClockReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data) {
  if (anon_limiter.allow(rtc_clock.getCurrentTime())) {
    // request data has: {reply-path-len}{reply-path}
    reply_path_len = *data++;
    if (!mesh::Packet::isValidPathLen(reply_path_len)) return 0;  // reject - bad encoding

    mesh::Packet::writePath(reply_path, data, reply_path_len);
    // data += (uint8_t)reply_path_len * reply_path_hash_size;

    memcpy(reply_data, &sender_timestamp, 4);   // prefix with sender_timestamp, like a tag
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply_data[4], &now, 4);     // include our clock (for easy clock sync, and packet hash uniqueness)
    reply_data[8] = 0;  // features
#ifdef WITH_RS232_BRIDGE
    reply_data[8] |= 0x01;  // is bridge, type UART
#elif WITH_ESPNOW_BRIDGE
    reply_data[8] |= 0x03;  // is bridge, type ESP-NOW
#endif
    if (_prefs.disable_fwd) {   // is this repeater currently disabled
      reply_data[8] |= 0x80;  // is disabled
    }
    // TODO:  add some kind of moving-window utilisation metric, so can query 'how busy' is this repeater
    return 9;   // reply length
  }
  return 0;
}

int MyMesh::handleRequest(ClientInfo *sender, uint32_t sender_timestamp, uint8_t *payload, size_t payload_len) {
  // uint32_t now = getRTCClock()->getCurrentTimeUnique();
  // memcpy(reply_data, &now, 4);   // response packets always prefixed with timestamp
  memcpy(reply_data, &sender_timestamp, 4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

  if (payload[0] == REQ_TYPE_GET_STATUS) {  // guests can also access this now
    RepeaterStats stats;
    stats.batt_milli_volts = board.getBattMilliVolts();
    stats.curr_tx_queue_len = _mgr->getOutboundTotal();
    stats.noise_floor = (int16_t)_radio->getNoiseFloor();
    stats.last_rssi = (int16_t)radio_driver.getLastRSSI();
    stats.n_packets_recv = radio_driver.getPacketsRecv();
    stats.n_packets_sent = radio_driver.getPacketsSent();
    stats.total_air_time_secs = getTotalAirTime() / 1000;
    stats.total_up_time_secs = uptime_millis / 1000;
    stats.n_sent_flood = getNumSentFlood();
    stats.n_sent_direct = getNumSentDirect();
    stats.n_recv_flood = getNumRecvFlood();
    stats.n_recv_direct = getNumRecvDirect();
    stats.err_events = _err_flags;
    stats.last_snr = (int16_t)(radio_driver.getLastSNR() * 4);
    stats.n_direct_dups = ((SimpleMeshTables *)getTables())->getNumDirectDups();
    stats.n_flood_dups = ((SimpleMeshTables *)getTables())->getNumFloodDups();
    stats.total_rx_air_time_secs = getReceiveAirTime() / 1000;
    stats.n_recv_errors = radio_driver.getPacketsRecvErrors();
    memcpy(&reply_data[4], &stats, sizeof(stats));

    return 4 + sizeof(stats); //  reply_len
  }
  if (payload[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t perm_mask = ~(payload[1]); // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions

    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);

    // query other sensors -- target specific
    if ((sender->permissions & PERM_ACL_ROLE_MASK) == PERM_ACL_GUEST) {
      perm_mask = 0x00;  // just base telemetry allowed
    }
    sensors.querySensors(perm_mask, telemetry);

	// This default temperature will be overridden by external sensors (if any)
    float temperature = board.getMCUTemperature();
    if(!isnan(temperature)) { // Supported boards with built-in temperature sensor. ESP32-C3 may return NAN
      telemetry.addTemperature(TELEM_CHANNEL_SELF, temperature); // Built-in MCU Temperature
    }

    uint8_t tlen = telemetry.getSize();
    memcpy(&reply_data[4], telemetry.getBuffer(), tlen);
    return 4 + tlen; // reply_len
  }
  if (payload[0] == REQ_TYPE_GET_ACCESS_LIST && sender->isAdmin()) {
    uint8_t res1 = payload[1];   // reserved for future  (extra query params)
    uint8_t res2 = payload[2];
    if (res1 == 0 && res2 == 0) {
      uint8_t ofs = 4;
      for (int i = 0; i < acl.getNumClients() && ofs + 7 <= sizeof(reply_data) - 4; i++) {
        auto c = acl.getClientByIdx(i);
        if (c->permissions == 0) continue;  // skip deleted entries
        memcpy(&reply_data[ofs], c->id.pub_key, 6); ofs += 6;  // just 6-byte pub_key prefix
        reply_data[ofs++] = c->permissions;
      }
      return ofs;
    }
  }
  if (payload[0] == REQ_TYPE_GET_NEIGHBOURS) {
    uint8_t request_version = payload[1];
    if (request_version == 0) {

      // reply data offset (after response sender_timestamp/tag)
      int reply_offset = 4;

      // get request params
      uint8_t count = payload[2]; // how many neighbours to fetch (0-255)
      uint16_t offset;
      memcpy(&offset, &payload[3], 2); // offset from start of neighbours list (0-65535)
      uint8_t order_by = payload[5]; // how to order neighbours. 0=newest_to_oldest, 1=oldest_to_newest, 2=strongest_to_weakest, 3=weakest_to_strongest
      uint8_t pubkey_prefix_length = payload[6]; // how many bytes of neighbour pub key we want
      // we also send a 4 byte random blob in payload[7...10] to help packet uniqueness

      MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS count=%d, offset=%d, order_by=%d, pubkey_prefix_length=%d", count, offset, order_by, pubkey_prefix_length);

      // clamp pub key prefix length to max pub key length
      if(pubkey_prefix_length > PUB_KEY_SIZE){
        pubkey_prefix_length = PUB_KEY_SIZE;
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS invalid pubkey_prefix_length=%d clamping to %d", pubkey_prefix_length, PUB_KEY_SIZE);
      }

      // create copy of neighbours list, skipping empty entries so we can sort it separately from main list
      int16_t neighbours_count = 0;
#if MAX_NEIGHBOURS
      NeighbourInfo* sorted_neighbours[MAX_NEIGHBOURS];
      for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        auto neighbour = &neighbours[i];
        if (neighbour->heard_timestamp > 0) {
          sorted_neighbours[neighbours_count] = neighbour;
          neighbours_count++;
        }
      }

      // sort neighbours based on order
      if (order_by == 0) {
        // sort by newest to oldest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting newest to oldest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->heard_timestamp > b->heard_timestamp; // desc
        });
      } else if (order_by == 1) {
        // sort by oldest to newest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting oldest to newest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->heard_timestamp < b->heard_timestamp; // asc
        });
      } else if (order_by == 2) {
        // sort by strongest to weakest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting strongest to weakest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->snr > b->snr; // desc
        });
      } else if (order_by == 3) {
        // sort by weakest to strongest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting weakest to strongest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->snr < b->snr; // asc
        });
      }
#endif

      // build results buffer
      int results_count = 0;
      int results_offset = 0;
      uint8_t results_buffer[130];
      for(int index = 0; index < count && index + offset < neighbours_count; index++){
        
        // stop if we can't fit another entry in results
        int entry_size = pubkey_prefix_length + 4 + 1;
        if(results_offset + entry_size > sizeof(results_buffer)){
          MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS no more entries can fit in results buffer");
          break;
        }

#if MAX_NEIGHBOURS
        // add next neighbour to results
        auto neighbour = sorted_neighbours[index + offset];
        uint32_t heard_seconds_ago = getRTCClock()->getCurrentTime() - neighbour->heard_timestamp;
        memcpy(&results_buffer[results_offset], neighbour->id.pub_key, pubkey_prefix_length); results_offset += pubkey_prefix_length;
        memcpy(&results_buffer[results_offset], &heard_seconds_ago, 4); results_offset += 4;
        memcpy(&results_buffer[results_offset], &neighbour->snr, 1); results_offset += 1;
        results_count++;
#endif

      }

      // build reply
      MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS neighbours_count=%d results_count=%d", neighbours_count, results_count);
      memcpy(&reply_data[reply_offset], &neighbours_count, 2); reply_offset += 2;
      memcpy(&reply_data[reply_offset], &results_count, 2); reply_offset += 2;
      memcpy(&reply_data[reply_offset], &results_buffer, results_offset); reply_offset += results_offset;

      return reply_offset;
    }
  } else if (payload[0] == REQ_TYPE_GET_OWNER_INFO) {
    sprintf((char *) &reply_data[4], "%s\n%s\n%s", FIRMWARE_VERSION, _prefs.node_name, _prefs.owner_info);
    return 4 + strlen((char *) &reply_data[4]);
  }
  return 0; // unknown command
}

mesh::Packet *MyMesh::createSelfAdvert() {
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len = _cli.buildAdvertData(ADV_TYPE_REPEATER, app_data);

  return createAdvert(self_id, app_data, app_data_len);
}

File MyMesh::openAppend(const char *fname) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return _fs->open(fname, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return _fs->open(fname, "a");
#else
  return _fs->open(fname, "a", true);
#endif
}

static uint8_t max_loop_minimal[] =  { 0, /* 1-byte */  4, /* 2-byte */  2, /* 3-byte */  1 };
static uint8_t max_loop_moderate[] = { 0, /* 1-byte */  2, /* 2-byte */  1, /* 3-byte */  1 };
static uint8_t max_loop_strict[] =   { 0, /* 1-byte */  1, /* 2-byte */  1, /* 3-byte */  1 };

bool MyMesh::isLooped(const mesh::Packet* packet, const uint8_t max_counters[]) {
  uint8_t hash_size = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  uint8_t n = 0;
  const uint8_t* path = packet->path;
  while (hash_count > 0) {      // count how many times this node is already in the path
    if (self_id.isHashMatch(path, hash_size)) n++;
    hash_count--;
    path += hash_size;
  }
  return n >= max_counters[hash_size];
}

void MyMesh::sendFloodReply(mesh::Packet* packet, unsigned long delay_millis, uint8_t path_hash_size) {
  TransportKey req_scope;
  bool is_wildcard = recv_pkt_region != NULL && recv_pkt_region->isWildcard();
  bool req_scope_known = recv_pkt_region != NULL && !is_wildcard
                      && region_map.getTransportKeysFor(*recv_pkt_region, &req_scope, 1) > 0;

  switch (mesh::chooseReplyScope(req_scope_known, is_wildcard, !default_scope.isNull())) {
    case mesh::REPLY_SCOPE_REQUEST:
      sendFloodScoped(req_scope, packet, delay_millis, path_hash_size);   // reply with same scope as request
      break;
    case mesh::REPLY_SCOPE_DEFAULT:
      // requester's scope is unknown: DIRECT request (no transport codes), or code matched no Region.
      // un-scoped would be dropped at hop 0 by repeaters running flood.max.unscoped=0
      sendFloodScoped(default_scope, packet, delay_millis, path_hash_size);
      break;
    case mesh::REPLY_SCOPE_NONE:
      sendFlood(packet, delay_millis, path_hash_size);  // send un-scoped
      break;
  }
}

#ifdef LOON_FIRMWARE
uint32_t MyMesh::calcLoonPrefsChecksum() const {
  return calcLoonChecksum(&loon_prefs, offsetof(LoonPrefs, checksum));
}

void MyMesh::resetLoonPrefs() {
  memset(&loon_prefs, 0, sizeof(loon_prefs));
  loon_prefs.magic = LOON_PREFS_MAGIC;
  loon_prefs.version = LOON_PREFS_VERSION;
  loon_prefs.daily_hour = 9;
  loon_prefs.timezone_minutes = -300; // America/Toronto standard-time default; configurable for DST
  loon_prefs.busy_threshold = 20;
  loon_prefs.max_busy_delay_secs = 120;
  loon_prefs.checksum = calcLoonPrefsChecksum();
}

void MyMesh::loadLoonPrefs() {
  resetLoonPrefs();
  if (!_fs->exists(LOON_PREFS_FILE)) return;
#if defined(RP2040_PLATFORM)
  File file = _fs->open(LOON_PREFS_FILE, "r");
#else
  File file = _fs->open(LOON_PREFS_FILE);
#endif
  if (!file) return;
  if (file.size() == sizeof(LoonPrefsV1)) {
    LoonPrefsV1 old;
    bool read_ok = file.read(reinterpret_cast<uint8_t*>(&old), sizeof(old)) == sizeof(old);
    file.close();
    bool valid = read_ok && old.magic == LOON_PREFS_MAGIC && old.version == 1 &&
                 old.checksum == calcLoonChecksum(&old, offsetof(LoonPrefsV1, checksum)) &&
                 old.ping_public <= 1 && old.ping_test <= 1 &&
                 old.announce_public <= LOON_ANNOUNCE_DAILY && old.announce_test <= LOON_ANNOUNCE_DAILY &&
                 old.daily_hour <= 23 && old.timezone_minutes >= -720 && old.timezone_minutes <= 840 &&
                 old.busy_threshold <= 100 && old.max_busy_delay_secs <= 3600;
    if (valid) {
      loon_prefs.ping_public = old.ping_public;
      loon_prefs.ping_test = old.ping_test;
      loon_prefs.announce_public = old.announce_public;
      loon_prefs.announce_test = old.announce_test;
      loon_prefs.daily_hour = old.daily_hour;
      loon_prefs.timezone_minutes = old.timezone_minutes;
      loon_prefs.busy_threshold = old.busy_threshold;
      loon_prefs.max_busy_delay_secs = old.max_busy_delay_secs;
      saveLoonPrefs();
    }
    return;
  }
  if (file.size() == sizeof(LoonPrefsV2)) {
    LoonPrefsV2 old;
    bool read_ok = file.read(reinterpret_cast<uint8_t*>(&old), sizeof(old)) == sizeof(old);
    file.close();
    bool valid = read_ok && old.magic == LOON_PREFS_MAGIC && old.version == 2 &&
                 old.checksum == calcLoonChecksum(&old, offsetof(LoonPrefsV2, checksum)) &&
                 old.ping_public <= 1 && old.ping_test <= 1 &&
                 old.announce_public <= LOON_ANNOUNCE_DAILY && old.announce_test <= LOON_ANNOUNCE_DAILY &&
                 old.daily_hour <= 23 && old.timezone_minutes >= -720 && old.timezone_minutes <= 840 &&
                 old.busy_threshold <= 100 && old.max_busy_delay_secs <= 3600 &&
                 old.announcement_message[sizeof(old.announcement_message) - 1] == 0;
    if (valid) {
      loon_prefs.ping_public = old.ping_public;
      loon_prefs.ping_test = old.ping_test;
      loon_prefs.announce_public = old.announce_public;
      loon_prefs.announce_test = old.announce_test;
      loon_prefs.daily_hour = old.daily_hour;
      loon_prefs.timezone_minutes = old.timezone_minutes;
      loon_prefs.busy_threshold = old.busy_threshold;
      loon_prefs.max_busy_delay_secs = old.max_busy_delay_secs;
      if (loonAnnouncementIsSafe(old.announcement_message)) {
        StrHelper::strncpy(loon_prefs.announcement_public_message, old.announcement_message,
                           sizeof(loon_prefs.announcement_public_message));
        StrHelper::strncpy(loon_prefs.announcement_test_message, old.announcement_message,
                           sizeof(loon_prefs.announcement_test_message));
      }
      saveLoonPrefs();
    }
    return;
  }
  if (file.size() != sizeof(loon_prefs)) { file.close(); return; }
  LoonPrefs loaded;
  if (file.read(reinterpret_cast<uint8_t*>(&loaded), sizeof(loaded)) != sizeof(loaded)) { file.close(); return; }
  file.close();
  bool valid_common = loaded.magic == LOON_PREFS_MAGIC &&
                      loaded.checksum == calcLoonChecksum(&loaded, offsetof(LoonPrefs, checksum)) &&
                      loaded.ping_public <= 1 && loaded.ping_test <= 1 &&
                      loaded.announce_public <= LOON_ANNOUNCE_DAILY &&
                      loaded.announce_test <= LOON_ANNOUNCE_DAILY && loaded.daily_hour <= 23 &&
                      loaded.timezone_minutes >= -720 && loaded.timezone_minutes <= 840 &&
                      loaded.busy_threshold <= 100 && loaded.max_busy_delay_secs <= 3600 &&
                      loaded.announcement_public_message[sizeof(loaded.announcement_public_message) - 1] == 0 &&
                      loaded.announcement_test_message[sizeof(loaded.announcement_test_message) - 1] == 0;
  LoonPrefs original = loon_prefs;
  loon_prefs = loaded;
  bool valid = valid_common && loon_prefs.version == LOON_PREFS_VERSION;
  if (!valid) loon_prefs = original;
  else if (!loonAnnouncementIsSafe(loon_prefs.announcement_public_message) ||
           !loonAnnouncementIsSafe(loon_prefs.announcement_test_message)) {
    if (!loonAnnouncementIsSafe(loon_prefs.announcement_public_message)) loon_prefs.announcement_public_message[0] = 0;
    if (!loonAnnouncementIsSafe(loon_prefs.announcement_test_message)) loon_prefs.announcement_test_message[0] = 0;
    saveLoonPrefs();
  }
}

void MyMesh::saveLoonPrefs() {
  loon_prefs.checksum = calcLoonPrefsChecksum();
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  _fs->remove(LOON_PREFS_FILE);
  File file = _fs->open(LOON_PREFS_FILE, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  File file = _fs->open(LOON_PREFS_FILE, "w");
#else
  File file = _fs->open(LOON_PREFS_FILE, "w", true);
#endif
  if (file) {
    file.write(reinterpret_cast<const uint8_t*>(&loon_prefs), sizeof(loon_prefs));
    file.close();
  }
}

#if defined(ESP32)
uint32_t MyMesh::calcLoonWebhookPrefsChecksum() const {
  return calcLoonChecksum(&loon_webhook_prefs, offsetof(LoonWebhookPrefs, checksum));
}

void MyMesh::resetLoonWebhookPrefs() {
  memset(&loon_webhook_prefs, 0, sizeof(loon_webhook_prefs));
  loon_webhook_prefs.magic = LOON_WEBHOOK_PREFS_MAGIC;
  loon_webhook_prefs.version = LOON_WEBHOOK_PREFS_VERSION;
  StrHelper::strncpy(loon_webhook_prefs.channel_name, "#test", sizeof(loon_webhook_prefs.channel_name));
  loon_webhook_prefs.checksum = calcLoonWebhookPrefsChecksum();
}

void MyMesh::loadLoonWebhookPrefs() {
  resetLoonWebhookPrefs();
  if (!_fs->exists(LOON_WEBHOOK_PREFS_FILE)) return;
  File file = _fs->open(LOON_WEBHOOK_PREFS_FILE);
  if (!file || file.size() != sizeof(loon_webhook_prefs)) {
    if (file) file.close();
    return;
  }
  LoonWebhookPrefs loaded;
  bool read_ok = file.read(reinterpret_cast<uint8_t*>(&loaded), sizeof(loaded)) == sizeof(loaded);
  file.close();
  bool valid = read_ok && loaded.magic == LOON_WEBHOOK_PREFS_MAGIC &&
               loaded.version == LOON_WEBHOOK_PREFS_VERSION &&
               loaded.checksum == calcLoonChecksum(&loaded, offsetof(LoonWebhookPrefs, checksum)) &&
               loaded.wifi_ssid[sizeof(loaded.wifi_ssid) - 1] == 0 &&
               loaded.wifi_password[sizeof(loaded.wifi_password) - 1] == 0 &&
               loaded.discord_webhook_url[sizeof(loaded.discord_webhook_url) - 1] == 0 &&
               loaded.channel_name[sizeof(loaded.channel_name) - 1] == 0;
  if (valid) loon_webhook_prefs = loaded;
}

void MyMesh::saveLoonWebhookPrefs() {
  loon_webhook_prefs.checksum = calcLoonWebhookPrefsChecksum();
  File file = _fs->open(LOON_WEBHOOK_PREFS_FILE, "w", true);
  if (file) {
    file.write(reinterpret_cast<const uint8_t*>(&loon_webhook_prefs), sizeof(loon_webhook_prefs));
    file.close();
  }
}

void MyMesh::initLoonWebhookChannel() {
  memset(&loon_webhook_channel, 0, sizeof(loon_webhook_channel));
  loon_webhook_channel_ready = false;
  if (!loon_webhook_prefs.channel_name[0]) return;
  // MeshCore's Public channel uses its standard fixed key, not the key that
  // would be produced by hashing the literal channel name "Public".
  if (!strcasecmp(loon_webhook_prefs.channel_name, "Public")) {
    memcpy(loon_webhook_channel.secret, LOON_PUBLIC_SECRET, sizeof(LOON_PUBLIC_SECRET));
  } else {
    mesh::Utils::sha256(loon_webhook_channel.secret, CIPHER_KEY_SIZE,
                        reinterpret_cast<const uint8_t*>(loon_webhook_prefs.channel_name),
                        strlen(loon_webhook_prefs.channel_name));
  }
  mesh::Utils::sha256(loon_webhook_channel.hash, sizeof(loon_webhook_channel.hash),
                      loon_webhook_channel.secret, CIPHER_KEY_SIZE);
  loon_webhook_channel_ready = true;
}

void MyMesh::initLoonWifi() {
  if (!loon_webhook_prefs.wifi_ssid[0] || !loon_webhook_prefs.wifi_password[0]) return;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(loon_webhook_prefs.wifi_ssid, loon_webhook_prefs.wifi_password);
  loon_next_wifi_attempt_at = futureMillis(LOON_WIFI_RETRY_DELAY_MS);
}

bool MyMesh::isLoonWebhookChannel(const mesh::GroupChannel& channel) const {
  return loon_webhook_channel_ready &&
         memcmp(channel.hash, loon_webhook_channel.hash, PATH_HASH_SIZE) == 0;
}

void MyMesh::queueLoonWebhook(const char* sender, const char* body) {
  if (!loon_webhook_prefs.discord_webhook_url[0] || !loon_webhook_channel_ready) return;
  if (loon_webhook_count >= LOON_WEBHOOK_QUEUE_SIZE) {
    loon_webhook_head = (uint8_t)((loon_webhook_head + 1) % LOON_WEBHOOK_QUEUE_SIZE);
    loon_webhook_count--;
  }
  LoonWebhookItem* item = &loon_webhook_queue[loon_webhook_tail];
  StrHelper::strncpy(item->sender, sender && sender[0] ? sender : "Unknown", sizeof(item->sender));
  StrHelper::strncpy(item->body, body ? body : "", sizeof(item->body));
  loon_webhook_tail = (uint8_t)((loon_webhook_tail + 1) % LOON_WEBHOOK_QUEUE_SIZE);
  loon_webhook_count++;
  loon_next_webhook_attempt_at = 0;
}

void MyMesh::queueLoonWebhookMessage(const mesh::GroupChannel& channel, const char* sender, const char* body) {
  if (!isLoonWebhookChannel(channel)) return;
  queueLoonWebhook(sender, body);
}

void MyMesh::pumpLoonWebhook() {
  if (!loon_webhook_count) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (loon_next_wifi_attempt_at && millisHasNowPassed(loon_next_wifi_attempt_at)) initLoonWifi();
    return;
  }
  if (loon_next_webhook_attempt_at && !millisHasNowPassed(loon_next_webhook_attempt_at)) return;

  LoonWebhookItem* item = &loon_webhook_queue[loon_webhook_head];
  char escaped_sender[80];
  char escaped_body[640];
  loonJsonEscape(item->sender, escaped_sender, sizeof(escaped_sender));
  loonJsonEscape(item->body, escaped_body, sizeof(escaped_body));
  char payload[896];
  snprintf(payload, sizeof(payload),
           "{\"username\":\"%s\",\"content\":\"%s\",\"allowed_mentions\":{\"parse\":[]}}",
           escaped_sender, escaped_body);

  char url_buf[sizeof(loon_webhook_prefs.discord_webhook_url)];
  StrHelper::strncpy(url_buf, loon_webhook_prefs.discord_webhook_url, sizeof(url_buf));
  char* url = url_buf;
  while (*url && (uint8_t)*url <= ' ') url++;
  char* end = url + strlen(url);
  while (end > url && (uint8_t)end[-1] <= ' ') *--end = 0;
  const char* scheme = strstr(url, "https://");
  if (scheme != url) {
    loon_next_webhook_attempt_at = futureMillis(LOON_WEBHOOK_RETRY_DELAY_MS);
    return;
  }
  const char* host = scheme + 8;
  const char* path = strchr(host, '/');
  char host_buf[128];
  if (path) {
    size_t host_len = min((size_t)(path - host), sizeof(host_buf) - 1);
    memcpy(host_buf, host, host_len);
    host_buf[host_len] = 0;
  } else {
    StrHelper::strncpy(host_buf, host, sizeof(host_buf));
    path = "/";
  }

  int code = -1;
  bool request_sent = false;
  WiFiClientSecure client;
  client.setInsecure();
  if (client.connect(host_buf, 443)) {
    client.printf("POST %s HTTP/1.1\r\n", path);
    client.printf("Host: %s\r\n", host_buf);
    client.print("User-Agent: Loon-Firmware\r\n");
    client.print("Content-Type: application/json\r\n");
    client.printf("Content-Length: %u\r\n", (unsigned)strlen(payload));
    client.print("Connection: close\r\n\r\n");
    size_t payload_len = strlen(payload);
    request_sent = client.write(reinterpret_cast<const uint8_t*>(payload), payload_len) == payload_len;
    String status = client.readStringUntil('\n');
    status.trim();
    if (status.startsWith("HTTP/")) {
      int space = status.indexOf(' ');
      if (space > 0) code = status.substring(space + 1).toInt();
    }
    client.stop();
  }

  // Once the complete request has left Loon, an absent/malformed response is
  // ambiguous: Discord may already have created the message. Retrying that item
  // is what produces duplicate posts when the response is lost over Wi-Fi.
  if ((code >= 200 && code < 300) || (request_sent && code < 0)) {
    loon_webhook_head = (uint8_t)((loon_webhook_head + 1) % LOON_WEBHOOK_QUEUE_SIZE);
    loon_webhook_count--;
    loon_next_webhook_attempt_at = futureMillis(LOON_WEBHOOK_MIN_INTERVAL_MS);
  } else {
    loon_next_webhook_attempt_at = futureMillis(LOON_WEBHOOK_RETRY_DELAY_MS);
  }
}
#endif

void MyMesh::initLoonChannels() {
  memset(&loon_public_channel, 0, sizeof(loon_public_channel));
  memcpy(loon_public_channel.secret, LOON_PUBLIC_SECRET, sizeof(LOON_PUBLIC_SECRET));
  mesh::Utils::sha256(loon_public_channel.hash, sizeof(loon_public_channel.hash),
                      loon_public_channel.secret, sizeof(LOON_PUBLIC_SECRET));
  loon_public_ready = true;

  memset(&loon_test_channel, 0, sizeof(loon_test_channel));
  memcpy(loon_test_channel.secret, LOON_TEST_SECRET, sizeof(LOON_TEST_SECRET));
  mesh::Utils::sha256(loon_test_channel.hash, sizeof(loon_test_channel.hash),
                      loon_test_channel.secret, sizeof(LOON_TEST_SECRET));
  loon_test_ready = true;
}

uint8_t MyMesh::calcLoonBusyPercent() {
  uint32_t now = millis();
  uint32_t tx = getTotalAirTime();
  uint32_t rx = getReceiveAirTime();
  if (!loon_busy_sample_at) {
    loon_busy_sample_at = now;
    loon_busy_tx_at = tx;
    loon_busy_rx_at = rx;
    return loon_busy_percent;
  }
  uint32_t elapsed = now - loon_busy_sample_at;
  if (elapsed >= 60000UL) {
    uint32_t airtime = (tx - loon_busy_tx_at) + (rx - loon_busy_rx_at);
    uint32_t percent = elapsed ? (airtime * 100UL) / elapsed : 0;
    loon_busy_percent = min((uint32_t)100, percent);
    loon_busy_sample_at = now;
    loon_busy_tx_at = tx;
    loon_busy_rx_at = rx;
  }
  return loon_busy_percent;
}

uint32_t MyMesh::calcLoonBusyDelay(uint8_t busy) const {
  if (busy <= loon_prefs.busy_threshold || loon_prefs.busy_threshold >= 100 || !loon_prefs.max_busy_delay_secs) return 0;
  uint32_t span = 100U - loon_prefs.busy_threshold;
  uint32_t over = busy - loon_prefs.busy_threshold;
  return (over * over * (uint32_t)loon_prefs.max_busy_delay_secs * 1000UL) / (span * span);
}

bool MyMesh::isLoonChannel(const mesh::GroupChannel& channel, bool& is_public) const {
  if (loon_public_ready && memcmp(channel.hash, loon_public_channel.hash, PATH_HASH_SIZE) == 0) {
    is_public = true;
    return true;
  }
  if (loon_test_ready && memcmp(channel.hash, loon_test_channel.hash, PATH_HASH_SIZE) == 0) {
    is_public = false;
    return true;
  }
  return false;
}

int MyMesh::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
  int count = 0;
  if (max_matches > count && loon_public_ready && memcmp(hash, loon_public_channel.hash, PATH_HASH_SIZE) == 0)
    channels[count++] = loon_public_channel;
  if (max_matches > count && loon_test_ready && memcmp(hash, loon_test_channel.hash, PATH_HASH_SIZE) == 0)
    channels[count++] = loon_test_channel;
#if defined(ESP32)
  bool already_added = (loon_public_ready && memcmp(hash, loon_public_channel.hash, PATH_HASH_SIZE) == 0) ||
                       (loon_test_ready && memcmp(hash, loon_test_channel.hash, PATH_HASH_SIZE) == 0);
  if (max_matches > count && !already_added && loon_webhook_channel_ready &&
      memcmp(hash, loon_webhook_channel.hash, PATH_HASH_SIZE) == 0)
    channels[count++] = loon_webhook_channel;
#endif
  return count;
}

void MyMesh::sendLoonPing(const mesh::GroupChannel& channel, const char* sender, const mesh::Packet* packet) {
  char path[196];
  formatLoonPath(packet, path, sizeof(path));
  uint8_t busy = calcLoonBusyPercent();
  uint32_t delay_ms = calcLoonBusyDelay(busy);
  uint8_t temp[MAX_PACKET_PAYLOAD];
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &timestamp, 4);
  temp[4] = 0;
  snprintf(reinterpret_cast<char*>(&temp[5]), LOON_MAX_ANNOUNCEMENT_TEXT + 1,
           "%s: 🏓 @[%s] | Path %s | RSSI %d | SNR %.1f | Busy %u%%",
           _prefs.node_name, sender, path, (int)_radio->getLastRSSI(), packet->getSNR(), busy);
  size_t len = strlen(reinterpret_cast<char*>(&temp[5]));
  mesh::Packet* reply = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel, temp, 5 + len);
  if (reply) {
    sendFlood(reply, SERVER_RESPONSE_DELAY + delay_ms, 3);
#if defined(ESP32)
    const char* full = reinterpret_cast<char*>(&temp[5]);
    const char* sep = strstr(full, ": ");
    queueLoonWebhookMessage(channel, _prefs.node_name, sep ? sep + 2 : full);
#endif
  }
}

void MyMesh::sendLoonReply(const mesh::GroupChannel& channel, const char* text) {
  uint8_t busy = calcLoonBusyPercent();
  uint32_t delay_ms = calcLoonBusyDelay(busy);
  uint8_t temp[MAX_PACKET_PAYLOAD];
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &timestamp, 4);
  temp[4] = 0;
  snprintf(reinterpret_cast<char*>(&temp[5]), LOON_MAX_ANNOUNCEMENT_TEXT + 1,
           "%s: %s", _prefs.node_name, text);
  size_t len = strlen(reinterpret_cast<char*>(&temp[5]));
  mesh::Packet* reply = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel, temp, 5 + len);
  if (reply) {
    sendFlood(reply, SERVER_RESPONSE_DELAY + delay_ms, 3);
#if defined(ESP32)
    queueLoonWebhookMessage(channel, _prefs.node_name, text);
#endif
  }
}

void MyMesh::onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                             uint8_t* data, size_t len) {
  if (type != PAYLOAD_TYPE_GRP_TXT || len <= 5 || len >= MAX_PACKET_PAYLOAD) return;
  bool is_public = false;
  bool is_loon_channel = isLoonChannel(channel, is_public);
#if defined(ESP32)
  bool is_webhook_channel = isLoonWebhookChannel(channel);
  if (!is_loon_channel && !is_webhook_channel) return;
#else
  if (!is_loon_channel) return;
#endif
  if ((data[4] >> 2) != TXT_TYPE_PLAIN) return;
  data[len] = 0;
  char* text = reinterpret_cast<char*>(&data[5]);
  char* sep = strstr(text, ": ");
  char sender[40];
  sender[0] = 0;
  const char* body = text;
  if (sep) {
    size_t sender_len = min((size_t)(sep - text), sizeof(sender) - 1);
    memcpy(sender, text, sender_len);
    sender[sender_len] = 0;
    body = sep + 2;
  }
  if (!sender[0]) StrHelper::strncpy(sender, "Unknown", sizeof(sender));
#if defined(ESP32)
  if (is_webhook_channel) queueLoonWebhookMessage(channel, sender, body);
#endif
  if (!is_loon_channel) return;
  if (strcmp(sender, _prefs.node_name) == 0) return;
  bool is_ping = loonCommandIs(body, "!ping");
  bool is_help = !is_public && loonCommandIs(body, "!help");
  bool is_about = !is_public && loonCommandIs(body, "!about");
  bool is_roll = !is_public && loonCommandIs(body, "!roll");
  bool is_rps = !is_public && loonCommandIs(body, "!rps");
  bool is_rps3 = !is_public && loonCommandIs(body, "!rps3");
  if (!is_ping && !is_help && !is_about && !is_roll && !is_rps && !is_rps3) return;
  if (is_ping && !(is_public ? loon_prefs.ping_public : loon_prefs.ping_test)) return;
  unsigned long now = millis();
  uint32_t sender_hash = calcLoonChecksum(sender, strlen(sender));
  uint8_t sender_slot = 0;
  unsigned long oldest_time = ~0UL;
  for (uint8_t i = 0; i < LOON_COMMAND_SENDER_SLOTS; i++) {
    if (loon_command_sender_hashes[i] == sender_hash && loon_command_sender_times[i]) {
      if ((uint32_t)(now - loon_command_sender_times[i]) < LOON_COMMAND_COOLDOWN_MS) return;
      sender_slot = i;
      oldest_time = 0;
      break;
    }
    if (!loon_command_sender_times[i]) {
      sender_slot = i;
      oldest_time = 0;
      break;
    }
    if (loon_command_sender_times[i] < oldest_time) {
      oldest_time = loon_command_sender_times[i];
      sender_slot = i;
    }
  }
  loon_command_sender_hashes[sender_slot] = sender_hash;
  loon_command_sender_times[sender_slot] = now ? now : 1;
  if (is_ping) {
    sendLoonPing(channel, sender, packet);
  } else if (is_help) {
    sendLoonReply(channel, "Commands: ping, roll, rps, rps3, about. Use the ! prefix.");
  } else if (is_roll) {
    char result[LOON_MAX_ANNOUNCEMENT_TEXT + 1];
    uint32_t value = getRNG()->nextInt(1, 7);
    snprintf(result, sizeof(result), "🎲 %lu", (unsigned long)value);
    sendLoonReply(channel, result);
  } else if (is_rps) {
    static const char* choices[] = {"🪨", "📄", "✂️"};
    sendLoonReply(channel, choices[getRNG()->nextInt(0, 3)]);
  } else if (is_rps3) {
    if (!loon_rps3_remaining) {
      loon_rps3_remaining = 3;
      loon_next_rps3_throw = futureMillis(10000UL);
    }
  } else {
    char about[LOON_MAX_ANNOUNCEMENT_TEXT + 1];
    if (_prefs.owner_info[0]) snprintf(about, sizeof(about), "%s | %s", FIRMWARE_VERSION, _prefs.owner_info);
    else StrHelper::strncpy(about, FIRMWARE_VERSION, sizeof(about));
    sendLoonReply(channel, about);
  }
}

unsigned long MyMesh::nextLoonAnnouncement(uint8_t mode) const {
  if (mode == LOON_ANNOUNCE_OFF) return 0;
  uint32_t epoch = getRTCClock()->getCurrentTime();
  if (epoch < LOON_VALID_CLOCK) return futureMillis(60000UL);
  int64_t local = (int64_t)epoch + (int64_t)loon_prefs.timezone_minutes * 60;
  uint32_t delta;
  if (mode == LOON_ANNOUNCE_HOURLY) {
    delta = 3600UL - (uint32_t)(local % 3600);
  } else {
    int64_t day = local / 86400;
    int64_t target = day * 86400 + (int64_t)loon_prefs.daily_hour * 3600;
    if (target <= local) target += 86400;
    delta = (uint32_t)(target - local);
  }
  uint32_t jitter = getRNG()->nextInt(0, 30001);
  return futureMillis(delta * 1000UL + jitter);
}

void MyMesh::scheduleLoonAnnouncements() {
  loon_next_public_announcement = nextLoonAnnouncement(loon_prefs.announce_public);
  loon_next_test_announcement = nextLoonAnnouncement(loon_prefs.announce_test);
}

void MyMesh::sendLoonAnnouncement(const mesh::GroupChannel& channel) {
  uint8_t busy = calcLoonBusyPercent();
  if (busy >= 80) return;
  uint8_t temp[MAX_PACKET_PAYLOAD];
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &timestamp, 4);
  temp[4] = 0;
  const char* message = (loon_public_ready &&
                         memcmp(channel.hash, loon_public_channel.hash, PATH_HASH_SIZE) == 0)
                            ? loon_prefs.announcement_public_message
                            : loon_prefs.announcement_test_message;
  if (message[0] && loonAnnouncementIsSafe(message)) {
    snprintf(reinterpret_cast<char*>(&temp[5]), LOON_MAX_ANNOUNCEMENT_TEXT + 1,
             "%s: %s", _prefs.node_name, message);
  } else {
    uint64_t seconds = uptime_millis / 1000ULL;
    unsigned long days = seconds / 86400ULL;
    unsigned long hours = (seconds % 86400ULL) / 3600ULL;
    snprintf(reinterpret_cast<char*>(&temp[5]), LOON_MAX_ANNOUNCEMENT_TEXT + 1,
             "%s: ONLINE | Up %lud%02luh | RX %lu | Repeated %lu | Busy %u%%",
             _prefs.node_name, days, hours, (unsigned long)radio_driver.getPacketsRecv(),
             (unsigned long)getNumSentFlood(), busy);
  }
  size_t len = strlen(reinterpret_cast<char*>(&temp[5]));
  mesh::Packet* pkt = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel, temp, 5 + len);
  if (pkt) {
    sendFlood(pkt, SERVER_RESPONSE_DELAY, 3);
#if defined(ESP32)
    const char* full = reinterpret_cast<char*>(&temp[5]);
    const char* sep = strstr(full, ": ");
    queueLoonWebhookMessage(channel, _prefs.node_name, sep ? sep + 2 : full);
#endif
  }
}
#endif

bool MyMesh::allowPacketForward(const mesh::Packet *packet) {
  if (_prefs.disable_fwd) return false;
  if (packet->isRouteFlood()
      && mesh::isFloodHopLimitExceeded(packet, _prefs.flood_max, _prefs.flood_max_unscoped, _prefs.flood_max_advert)) {
    return false;
  }
  if (packet->isRouteFlood() && recv_pkt_region == NULL) {
    MESH_DEBUG_PRINTLN("allowPacketForward: unknown transport code, or wildcard not allowed for FLOOD packet");
    return false;
  }
  if (packet->isRouteFlood() && _prefs.loop_detect != LOOP_DETECT_OFF) {
    const uint8_t* maximums;
    if (_prefs.loop_detect == LOOP_DETECT_MINIMAL) {
      maximums = max_loop_minimal;
    } else if (_prefs.loop_detect == LOOP_DETECT_MODERATE) {
      maximums = max_loop_moderate;
    } else {
      maximums = max_loop_strict;
    }
    if (isLooped(packet, maximums)) {
      MESH_DEBUG_PRINTLN("allowPacketForward: FLOOD packet loop detected!");
      return false;
    }
  }
  return true;
}

const char *MyMesh::getLogDateTime() {
  static char tmp[32];
  uint32_t now = getRTCClock()->getCurrentTime();
  DateTime dt = DateTime(now);
  sprintf(tmp, "%02d:%02d:%02d - %d/%d/%d U", dt.hour(), dt.minute(), dt.second(), dt.day(), dt.month(),
          dt.year());
  return tmp;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
#if MESH_PACKET_LOGGING
  Serial.print(getLogDateTime());
  Serial.print(" RAW: ");
  mesh::Utils::printHex(Serial, raw, len);
  Serial.println();
#endif
}

void MyMesh::logRx(mesh::Packet *pkt, int len, float score) {
#ifdef WITH_BRIDGE
  if (_prefs.bridge_pkt_src == 1) {
    bridge.sendPacket(pkt);
  }
#endif

  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": RX, len=%d (type=%d, route=%s, payload_len=%d) SNR=%d RSSI=%d score=%d", len,
               pkt->getPayloadType(), pkt->isRouteDirect() ? "D" : "F", pkt->payload_len,
               (int)_radio->getLastSNR(), (int)_radio->getLastRSSI(), (int)(score * 1000));

      if (pkt->getPayloadType() == PAYLOAD_TYPE_PATH || pkt->getPayloadType() == PAYLOAD_TYPE_REQ ||
          pkt->getPayloadType() == PAYLOAD_TYPE_RESPONSE || pkt->getPayloadType() == PAYLOAD_TYPE_TXT_MSG) {
        f.printf(" [%02X -> %02X]\n", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
      } else {
        f.printf("\n");
      }
      f.close();
    }
  }
}

void MyMesh::logTx(mesh::Packet *pkt, int len) {
#ifdef WITH_BRIDGE
  if (_prefs.bridge_pkt_src == 0) {
    bridge.sendPacket(pkt);
  }
#endif

  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": TX, len=%d (type=%d, route=%s, payload_len=%d)", len, pkt->getPayloadType(),
               pkt->isRouteDirect() ? "D" : "F", pkt->payload_len);

      if (pkt->getPayloadType() == PAYLOAD_TYPE_PATH || pkt->getPayloadType() == PAYLOAD_TYPE_REQ ||
          pkt->getPayloadType() == PAYLOAD_TYPE_RESPONSE || pkt->getPayloadType() == PAYLOAD_TYPE_TXT_MSG) {
        f.printf(" [%02X -> %02X]\n", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
      } else {
        f.printf("\n");
      }
      f.close();
    }
  }
}

void MyMesh::logTxFail(mesh::Packet *pkt, int len) {
  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": TX FAIL!, len=%d (type=%d, route=%s, payload_len=%d)\n", len, pkt->getPayloadType(),
               pkt->isRouteDirect() ? "D" : "F", pkt->payload_len);
      f.close();
    }
  }
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * _prefs.tx_delay_factor);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * _prefs.direct_tx_delay_factor);
  return getRNG()->nextInt(0, 5*t + 1);
}

mesh::DispatcherAction MyMesh::onRecvPacket(mesh::Packet* pkt) {
  if (pkt->getRouteType() == ROUTE_TYPE_TRANSPORT_FLOOD) {
    recv_pkt_region = region_map.findMatch(pkt, REGION_DENY_FLOOD);
  } else if (pkt->getRouteType() == ROUTE_TYPE_FLOOD) {
    if (region_map.getWildcard().flags & REGION_DENY_FLOOD) {
      recv_pkt_region = NULL;
    } else {
      recv_pkt_region =  &region_map.getWildcard();
    }
  } else {
    recv_pkt_region = NULL;
  }
  return Mesh::onRecvPacket(pkt);
}

void MyMesh::onAnonDataRecv(mesh::Packet *packet, const uint8_t *secret, const mesh::Identity &sender,
                            uint8_t *data, size_t len) {
  if (packet->getPayloadType() == PAYLOAD_TYPE_ANON_REQ) { // received an initial request by a possible admin
                                                           // client (unknown at this stage)
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    data[len] = 0;  // ensure null terminator
    uint8_t reply_len;

    reply_path_len = 0xFF;
    if (data[4] == 0 || data[4] >= ' ') {   // is password, ie. a login request
      reply_len = handleLoginReq(sender, secret, timestamp, &data[4], packet->isRouteFlood());
    } else if (data[4] == ANON_REQ_TYPE_REGIONS && packet->isRouteDirect()) {
      reply_len = handleAnonRegionsReq(sender, timestamp, &data[5]);
    } else if (data[4] == ANON_REQ_TYPE_OWNER && packet->isRouteDirect()) {
      reply_len = handleAnonOwnerReq(sender, timestamp, &data[5]);
    } else if (data[4] == ANON_REQ_TYPE_BASIC && packet->isRouteDirect()) {
      reply_len = handleAnonClockReq(sender, timestamp, &data[5]);
    } else {
      reply_len = 0;  // unknown/invalid request type
    }

    if (reply_len == 0) return;   // invalid request

    // a DIRECT login can reply via the stored out_path, as onPeerDataRecv() does for REQ
    ClientInfo* client = acl.getClient(sender.pub_key, PUB_KEY_SIZE);
    bool have_out_path = client != NULL && client->out_path_len != OUT_PATH_UNKNOWN;

    auto route = mesh::chooseReplyRoute(packet->isRouteFlood(), reply_path_len != 0xFF, have_out_path);

    if (route == mesh::REPLY_ROUTE_PATH_RETURN) {
      // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
      mesh::Packet* path = createPathReturn(sender, secret, packet->path, packet->path_len,
                                            PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
      if (path) sendFloodReply(path, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
      return;
    }

    mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, secret, reply_data, reply_len);
    if (reply == NULL) return;

    if (route == mesh::REPLY_ROUTE_DIRECT_SUPPLIED) {
      sendDirect(reply, reply_path, reply_path_len, SERVER_RESPONSE_DELAY);
    } else if (route == mesh::REPLY_ROUTE_DIRECT_OUT_PATH) {
      sendDirect(reply, client->out_path, client->out_path_len, SERVER_RESPONSE_DELAY);
    } else {
      sendFloodReply(reply, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
    }
  }
}

int MyMesh::searchPeersByHash(const uint8_t *hash) {
  int n = 0;
  for (int i = 0; i < acl.getNumClients(); i++) {
    if (acl.getClientByIdx(i)->id.isHashMatch(hash)) {
      matching_peer_indexes[n++] = i; // store the INDEXES of matching contacts (for subsequent 'peer' methods)
    }
  }
  return n;
}

void MyMesh::getPeerSharedSecret(uint8_t *dest_secret, int peer_idx) {
  int i = matching_peer_indexes[peer_idx];
  if (i >= 0 && i < acl.getNumClients()) {
    // lookup pre-calculated shared_secret
    memcpy(dest_secret, acl.getClientByIdx(i)->shared_secret, PUB_KEY_SIZE);
  } else {
    MESH_DEBUG_PRINTLN("getPeerSharedSecret: Invalid peer idx: %d", i);
  }
}

static bool isShare(const mesh::Packet *packet) {
  if (packet->hasTransportCodes()) {
    return packet->transport_codes[0] == 0 && packet->transport_codes[1] == 0;  // codes { 0, 0 } means 'send to nowhere'
  }
  return false;
}

void MyMesh::onAdvertRecv(mesh::Packet *packet, const mesh::Identity &id, uint32_t timestamp,
                          const uint8_t *app_data, size_t app_data_len) {
  mesh::Mesh::onAdvertRecv(packet, id, timestamp, app_data, app_data_len); // chain to super impl

  // if this a zero hop advert (and not via 'Share'), add it to neighbours
  if (packet->getPathHashCount() == 0 && !isShare(packet)) {
    AdvertDataParser parser(app_data, app_data_len);
    if (parser.isValid() && parser.getType() == ADV_TYPE_REPEATER) { // just keep neigbouring Repeaters
      putNeighbour(id, timestamp, packet->getSNR());
    }
  }
}

void MyMesh::onPeerDataRecv(mesh::Packet *packet, uint8_t type, int sender_idx, const uint8_t *secret,
                            uint8_t *data, size_t len) {
  int i = matching_peer_indexes[sender_idx];
  if (i < 0 || i >= acl.getNumClients()) { // get from our known_clients table (sender SHOULD already be known in this context)
    MESH_DEBUG_PRINTLN("onPeerDataRecv: invalid peer idx: %d", i);
    return;
  }
  ClientInfo* client = acl.getClientByIdx(i);

  if (type == PAYLOAD_TYPE_REQ) { // request (from a Known admin client!)
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    if (timestamp > client->last_timestamp) { // prevent replay attacks
      int reply_len = handleRequest(client, timestamp, &data[4], len - 4);
      if (reply_len == 0) return; // invalid command

      client->last_timestamp = timestamp;
      client->last_activity = getRTCClock()->getCurrentTime();

      if (packet->isRouteFlood()) {
        // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
        mesh::Packet *path = createPathReturn(client->id, secret, packet->path, packet->path_len,
                                              PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
        if (path) sendFloodReply(path, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
      } else {
        mesh::Packet *reply =
            createDatagram(PAYLOAD_TYPE_RESPONSE, client->id, secret, reply_data, reply_len);
        if (reply) {
          if (client->out_path_len != OUT_PATH_UNKNOWN) { // we have an out_path, so send DIRECT
            sendDirect(reply, client->out_path, client->out_path_len, SERVER_RESPONSE_DELAY);
          } else {
            sendFloodReply(reply, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  } else if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && client->isAdmin()) { // a CLI command
    uint32_t sender_timestamp;
    memcpy(&sender_timestamp, data, 4); // timestamp (by sender's RTC clock - which could be wrong)
    uint8_t flags = (data[4] >> 2);        // message attempt number, and other flags

    if (!(flags == TXT_TYPE_PLAIN || flags == TXT_TYPE_CLI_DATA)) {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: unsupported text type received: flags=%02x", (uint32_t)flags);
    } else if (sender_timestamp >= client->last_timestamp) { // prevent replay attacks
      bool is_retry = (sender_timestamp == client->last_timestamp);
      client->last_timestamp = sender_timestamp;
      client->last_activity = getRTCClock()->getCurrentTime();

      // len can be > original length, but 'text' will be padded with zeroes
      data[len] = 0; // need to make a C string again, with null terminator

      if (flags == TXT_TYPE_PLAIN) { // for legacy CLI, send Acks
        uint32_t ack_hash; // calc truncated hash of the message timestamp + text + sender pub_key, to prove
                           // to sender that we got it
        mesh::Utils::sha256((uint8_t *)&ack_hash, 4, data, 5 + strlen((char *)&data[5]), client->id.pub_key,
                            PUB_KEY_SIZE);

        mesh::Packet *ack = createAck(ack_hash);
        if (ack) {
          if (client->out_path_len == OUT_PATH_UNKNOWN) {
            sendFloodReply(ack, TXT_ACK_DELAY, packet->getPathHashSize());
          } else {
            sendDirect(ack, client->out_path, client->out_path_len, TXT_ACK_DELAY);
          }
        }
      }

      uint8_t temp[166];
      char *command = (char *)&data[5];
      char *reply = (char *)&temp[5];
      if (is_retry) {
        *reply = 0;
      } else {
        handleCommand(sender_timestamp, command, reply);
      }
      int text_len = strlen(reply);
      if (text_len > 0) {
        uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
        if (timestamp == sender_timestamp) {
          // WORKAROUND: the two timestamps need to be different, in the CLI view
          timestamp++;
        }
        memcpy(temp, &timestamp, 4);        // mostly an extra blob to help make packet_hash unique
        temp[4] = (TXT_TYPE_CLI_DATA << 2); // NOTE: legacy was: TXT_TYPE_PLAIN

        auto reply = createDatagram(PAYLOAD_TYPE_TXT_MSG, client->id, secret, temp, 5 + text_len);
        if (reply) {
          if (client->out_path_len == OUT_PATH_UNKNOWN) {
            sendFloodReply(reply, CLI_REPLY_DELAY_MILLIS, packet->getPathHashSize());
          } else {
            sendDirect(reply, client->out_path, client->out_path_len, CLI_REPLY_DELAY_MILLIS);
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  }
}

bool MyMesh::onPeerPathRecv(mesh::Packet *packet, int sender_idx, const uint8_t *secret, uint8_t *path,
                            uint8_t path_len, uint8_t extra_type, uint8_t *extra, uint8_t extra_len) {
  // TODO: prevent replay attacks
  int i = matching_peer_indexes[sender_idx];

  if (i >= 0 && i < acl.getNumClients()) { // get from our known_clients table (sender SHOULD already be known in this context)
    MESH_DEBUG_PRINTLN("PATH to client, path_len=%d", (uint32_t)path_len);
    auto client = acl.getClientByIdx(i);

    // store a copy of path, for sendDirect()
    client->out_path_len = mesh::Packet::copyPath(client->out_path, path, path_len);
    client->last_activity = getRTCClock()->getCurrentTime();
  } else {
    MESH_DEBUG_PRINTLN("onPeerPathRecv: invalid peer idx: %d", i);
  }

  // NOTE: no reciprocal path send!!
  return false;
}

#define CTL_TYPE_NODE_DISCOVER_REQ   0x80
#define CTL_TYPE_NODE_DISCOVER_RESP  0x90

void MyMesh::onControlDataRecv(mesh::Packet* packet) {
  uint8_t type = packet->payload[0] & 0xF0;    // just test upper 4 bits
  if (type == CTL_TYPE_NODE_DISCOVER_REQ && packet->payload_len >= 6
      && !_prefs.disable_fwd && discover_limiter.allow(rtc_clock.getCurrentTime())
  ) {
    int i = 1;
    uint8_t  filter = packet->payload[i++];
    uint32_t tag;
    memcpy(&tag, &packet->payload[i], 4); i += 4;
    uint32_t since;
    if (packet->payload_len >= i+4) {   // optional since field
      memcpy(&since, &packet->payload[i], 4); i += 4;
    } else {
      since = 0;
    }

    if ((filter & (1 << ADV_TYPE_REPEATER)) != 0 && _prefs.discovery_mod_timestamp >= since) {
      bool prefix_only = packet->payload[0] & 1;
      uint8_t data[6 + PUB_KEY_SIZE];
      data[0] = CTL_TYPE_NODE_DISCOVER_RESP | ADV_TYPE_REPEATER;   // low 4-bits for node type
      data[1] = packet->_snr;   // let sender know the inbound SNR ( x 4)
      memcpy(&data[2], &tag, 4);     // include tag from request, for client to match to
      memcpy(&data[6], self_id.pub_key, PUB_KEY_SIZE);
      auto resp = createControlData(data, prefix_only ? 6 + 8 : 6 + PUB_KEY_SIZE);
      if (resp) {
        sendZeroHop(resp, getRetransmitDelay(resp)*4);  // apply random delay (widened x4), as multiple nodes can respond to this
      }
    }
  } else if (type == CTL_TYPE_NODE_DISCOVER_RESP && packet->payload_len >= 6) {
    uint8_t node_type = packet->payload[0] & 0x0F;
    if (node_type != ADV_TYPE_REPEATER) {
      return;
    }
    if (packet->payload_len < 6 + PUB_KEY_SIZE) {
      MESH_DEBUG_PRINTLN("onControlDataRecv: DISCOVER_RESP pubkey too short: %d", (uint32_t)packet->payload_len);
      return;
    }

    if (pending_discover_tag == 0 || millisHasNowPassed(pending_discover_until)) {
      pending_discover_tag = 0;
      return;
    }
    uint32_t tag;
    memcpy(&tag, &packet->payload[2], 4);
    if (tag != pending_discover_tag) {
      return;
    }

    mesh::Identity id(&packet->payload[6]);
    if (id.matches(self_id)) {
      return;
    }
    putNeighbour(id, rtc_clock.getCurrentTime(), packet->getSNR());
  }
}

void MyMesh::sendNodeDiscoverReq() {
  uint8_t data[10];
  data[0] = CTL_TYPE_NODE_DISCOVER_REQ; // prefix_only=0
  data[1] = (1 << ADV_TYPE_REPEATER);
  getRNG()->random(&data[2], 4); // tag
  memcpy(&pending_discover_tag, &data[2], 4);
  pending_discover_until = futureMillis(60000);
  uint32_t since = 0;
  memcpy(&data[6], &since, 4);

  auto pkt = createControlData(data, sizeof(data));
  if (pkt) {
    sendZeroHop(pkt);
  }
}

MyMesh::MyMesh(mesh::MainBoard &board, mesh::Radio &radio, mesh::MillisecondClock &ms, mesh::RNG &rng,
               mesh::RTCClock &rtc, mesh::MeshTables &tables)
    : mesh::Mesh(radio, ms, rng, rtc, *new StaticPoolPacketManager(32), tables),
      region_map(key_store), temp_map(key_store),
      _cli(board, rtc, sensors, region_map, acl, &_prefs, this),
      telemetry(MAX_PACKET_PAYLOAD - 4),
      discover_limiter(4, 120),  // max 4 every 2 minutes
      anon_limiter(4, 180)   // max 4 every 3 minutes
#if defined(WITH_RS232_BRIDGE)
      , bridge(&_prefs, WITH_RS232_BRIDGE, _mgr, &rtc)
#endif
#if defined(WITH_ESPNOW_BRIDGE)
      , bridge(&_prefs, _mgr, &rtc)
#endif
{
  last_millis = 0;
  uptime_millis = 0;
  next_local_advert = next_flood_advert = 0;
  dirty_contacts_expiry = 0;
  set_radio_at = revert_radio_at = 0;
  _logging = false;
  region_load_active = false;
  recv_pkt_region = NULL;

#if MAX_NEIGHBOURS
  memset(neighbours, 0, sizeof(neighbours));
#endif

  // defaults
  _prefs.airtime_factor = 1.0;
  _prefs.rx_delay_base = 0.0f;   // turn off by default, was 10.0;
  _prefs.tx_delay_factor = 0.5f; // was 0.25f
  _prefs.direct_tx_delay_factor = 0.3f; // was 0.2
  StrHelper::strncpy(_prefs.node_name, ADVERT_NAME, sizeof(_prefs.node_name));
  _prefs.node_lat = ADVERT_LAT;
  _prefs.node_lon = ADVERT_LON;
  StrHelper::strncpy(_prefs.password, ADMIN_PASSWORD, sizeof(_prefs.password));
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.advert_interval = 1;        // default to 2 minutes for NEW installs
  _prefs.flood_advert_interval = 47; // 47 hours
  _prefs.flood_max = 64;
  _prefs.flood_max_unscoped = 64;
  _prefs.flood_max_advert = 8;
  _prefs.interference_threshold = 0; // disabled
  _prefs.cad_enabled = 0;            // hardware CAD before TX (off by default; 'set cad on')

  // bridge defaults
  _prefs.bridge_enabled = 1;    // enabled
  _prefs.bridge_delay   = 500;  // milliseconds
  _prefs.bridge_pkt_src = 0;    // logTx
  _prefs.bridge_baud = 115200;  // baud rate
  _prefs.bridge_channel = 1;    // channel 1

  StrHelper::strncpy(_prefs.bridge_secret, "LVSITANOS", sizeof(_prefs.bridge_secret));

  // GPS defaults
  _prefs.gps_enabled = 0;
  _prefs.gps_interval = 0;
  _prefs.advert_loc_policy = ADVERT_LOC_PREFS;

  _prefs.adc_multiplier = 0.0f; // 0.0f means use default board multiplier

#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default;
#endif
#endif
  _prefs.radio_fem_rxgain = 1;
  _prefs.radio_fem_txgain = 0;

  pending_discover_tag = 0;
  pending_discover_until = 0;

  memset(default_scope.key, 0, sizeof(default_scope.key));
#ifdef LOON_FIRMWARE
  resetLoonPrefs();
  loon_public_ready = loon_test_ready = false;
  loon_next_public_announcement = loon_next_test_announcement = 0;
  loon_next_rps3_throw = 0;
  loon_rps3_remaining = 0;
  memset(loon_command_sender_hashes, 0, sizeof(loon_command_sender_hashes));
  memset(loon_command_sender_times, 0, sizeof(loon_command_sender_times));
  loon_busy_sample_at = loon_busy_tx_at = loon_busy_rx_at = 0;
  loon_busy_percent = 0;
#if defined(ESP32)
  resetLoonWebhookPrefs();
  loon_webhook_channel_ready = false;
  loon_next_wifi_attempt_at = loon_next_webhook_attempt_at = 0;
  loon_webhook_head = loon_webhook_tail = loon_webhook_count = 0;
#endif
#endif
}

void MyMesh::begin(FILESYSTEM *fs) {
  mesh::Mesh::begin();
  _fs = fs;
  // load persisted prefs
  _cli.loadPrefs(_fs);
  acl.load(_fs, self_id);
  // TODO: key_store.begin();
  region_map.load(_fs);
#ifdef LOON_FIRMWARE
  loadLoonPrefs();
  initLoonChannels();
#if defined(ESP32)
  loadLoonWebhookPrefs();
  initLoonWebhookChannel();
  initLoonWifi();
#endif
#endif

  // establish default-scope
  {
    RegionEntry* r = region_map.getDefaultRegion();
    if (r) {
      region_map.getTransportKeysFor(*r, &default_scope, 1);
    } else {
#ifdef DEFAULT_FLOOD_SCOPE_NAME
      r = region_map.findByName(DEFAULT_FLOOD_SCOPE_NAME);
      if (r == NULL) {
        r = region_map.putRegion(DEFAULT_FLOOD_SCOPE_NAME, 0);  // auto-create the default scope region
        if (r) { r->flags = 0; }   // Allow-flood
      }
      if (r) {
        region_map.setDefaultRegion(r);
        region_map.getTransportKeysFor(*r, &default_scope, 1);
      }
#endif
    }
  }

#if defined(WITH_BRIDGE)
  if (_prefs.bridge_enabled) {
    bridge.begin();
  }
#endif

  radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_driver.setTxPower(_prefs.tx_power_dbm);

  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");
  board.setLoRaFemLnaEnabled(_prefs.radio_fem_rxgain);
  board.setLoRaFemPaGainEnabled(_prefs.radio_fem_txgain);

  updateAdvertTimer();
  updateFloodAdvertTimer();
#ifdef LOON_FIRMWARE
  scheduleLoonAnnouncements();
#endif

  board.setAdcMultiplier(_prefs.adc_multiplier);

#if ENV_INCLUDE_GPS == 1
  applyGpsPrefs();
#endif
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis, uint8_t path_hash_size) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, path_hash_size);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, path_hash_size);
  }
}

void MyMesh::applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) {
  set_radio_at = futureMillis(2000); // give CLI reply some time to be sent back, before applying temp radio params
  pending_freq = freq;
  pending_bw = bw;
  pending_sf = sf;
  pending_cr = cr;

  revert_radio_at = futureMillis(2000 + timeout_mins * 60 * 1000); // schedule when to revert radio params
}

bool MyMesh::formatFileSystem() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return InternalFS.format();
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  return SPIFFS.format();
#else
#error "need to implement file system erase"
  return false;
#endif
}

void MyMesh::sendSelfAdvertisement(int delay_millis, bool flood) {
  mesh::Packet *pkt = createSelfAdvert();
  if (pkt) {
    if (flood) {
      sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);
    } else {
      sendZeroHop(pkt, delay_millis);
    }
  } else {
    MESH_DEBUG_PRINTLN("ERROR: unable to create advertisement packet!");
  }
}

void MyMesh::updateAdvertTimer() {
  if (_prefs.advert_interval > 0) { // schedule local advert timer
    next_local_advert = futureMillis(((uint32_t)_prefs.advert_interval) * 2 * 60 * 1000);
  } else {
    next_local_advert = 0; // stop the timer
  }
}

void MyMesh::updateFloodAdvertTimer() {
  if (_prefs.flood_advert_interval > 0) { // schedule flood advert timer
    next_flood_advert = futureMillis(((uint32_t)_prefs.flood_advert_interval) * 60 * 60 * 1000);
  } else {
    next_flood_advert = 0; // stop the timer
  }
}

void MyMesh::dumpLogFile() {
#if defined(RP2040_PLATFORM)
  File f = _fs->open(PACKET_LOG_FILE, "r");
#else
  File f = _fs->open(PACKET_LOG_FILE);
#endif
  if (f) {
    while (f.available()) {
      int c = f.read();
      if (c < 0) break;
      Serial.print((char)c);
    }
    f.close();
  }
}

void MyMesh::setTxPower(int8_t power_dbm) {
  radio_driver.setTxPower(power_dbm);
}

bool MyMesh::setRxBoostedGain(bool enable) {
  return radio_driver.setRxBoostedGainMode(enable);
}

#if defined(USE_LR2021)
bool MyMesh::configSideDetectors(const uint8_t sideDetSFs[], uint8_t num, float bw) {
  return radio_driver.configSideDetectors(sideDetSFs, num, bw);
}
#endif

void MyMesh::formatNeighborsReply(char *reply) {
  char *dp = reply;

#if MAX_NEIGHBOURS
  // create copy of neighbours list, skipping empty entries so we can sort it separately from main list
  int16_t neighbours_count = 0;
  NeighbourInfo* sorted_neighbours[MAX_NEIGHBOURS];
  for (int i = 0; i < MAX_NEIGHBOURS; i++) {
    auto neighbour = &neighbours[i];
    if (neighbour->heard_timestamp > 0) {
      sorted_neighbours[neighbours_count] = neighbour;
      neighbours_count++;
    }
  }

  // sort neighbours newest to oldest
  std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
    return a->heard_timestamp > b->heard_timestamp; // desc
  });

  for (int i = 0; i < neighbours_count && dp - reply < 134; i++) {
    NeighbourInfo *neighbour = sorted_neighbours[i];

    // add new line if not first item
    if (i > 0) *dp++ = '\n';

    char hex[10];
    // get 4 bytes of neighbour id as hex
    mesh::Utils::toHex(hex, neighbour->id.pub_key, 4);

    // add next neighbour
    uint32_t secs_ago = getRTCClock()->getCurrentTime() - neighbour->heard_timestamp;
    sprintf(dp, "%s:%d:%d", hex, secs_ago, neighbour->snr);
    while (*dp)
      dp++; // find end of string
  }
#endif
  if (dp == reply) { // no neighbours, need empty response
    strcpy(dp, "-none-");
    dp += 6;
  }
  *dp = 0; // null terminator
}

void MyMesh::removeNeighbor(const uint8_t *pubkey, int key_len) {
#if MAX_NEIGHBOURS
  for (int i = 0; i < MAX_NEIGHBOURS; i++) {
    NeighbourInfo *neighbour = &neighbours[i];
    if (memcmp(neighbour->id.pub_key, pubkey, key_len) == 0) {
      neighbours[i] = NeighbourInfo(); // clear neighbour entry
    }
  }
#endif
}

void MyMesh::startRegionsLoad() {
  temp_map.resetFrom(region_map);   // rebuild regions in a temp instance
  memset(load_stack, 0, sizeof(load_stack));
  load_stack[0] = &temp_map.getWildcard();
  region_load_active = true;
}

bool MyMesh::saveRegions() {
  return region_map.save(_fs);
}

void MyMesh::onDefaultRegionChanged(const RegionEntry* r) {
  if (r) {
    region_map.getTransportKeysFor(*r, &default_scope, 1);
  } else {
    memset(default_scope.key, 0, sizeof(default_scope.key));
  }
}

void MyMesh::formatStatsReply(char *reply) {
  StatsFormatHelper::formatCoreStats(reply, board, *_ms, _err_flags, _mgr);
}

void MyMesh::formatRadioStatsReply(char *reply) {
  StatsFormatHelper::formatRadioStats(reply, _radio, radio_driver, getTotalAirTime(), getReceiveAirTime());
}

void MyMesh::formatPacketStatsReply(char *reply) {
  StatsFormatHelper::formatPacketStats(reply, radio_driver, getNumSentFlood(), getNumSentDirect(), 
                                       getNumRecvFlood(), getNumRecvDirect());
}

void MyMesh::saveIdentity(const mesh::LocalIdentity &new_id) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  IdentityStore store(*_fs, "");
#elif defined(ESP32)
  IdentityStore store(*_fs, "/identity");
#elif defined(RP2040_PLATFORM)
  IdentityStore store(*_fs, "/identity");
#else
#error "need to define saveIdentity()"
#endif
  store.save("_main", new_id);
}

void MyMesh::clearStats() {
  radio_driver.resetStats();
  resetStats();
  ((SimpleMeshTables *)getTables())->resetStats();
}

void MyMesh::handleCommand(uint32_t sender_timestamp, char *command, char *reply) {
  if (region_load_active) {
    if (StrHelper::isBlank(command)) {  // empty/blank line, signal to terminate 'load' operation
      region_map = temp_map;  // copy over the temp instance as new current map
      region_load_active = false;

      sprintf(reply, "OK - loaded %d regions", region_map.getCount());
    } else {
      char *np = command;
      while (*np == ' ') np++;   // skip indent
      int indent = np - command;

      char *ep = np;
      while (RegionMap::is_name_char(*ep)) ep++;
      if (*ep) { *ep++ = 0; }  // set null terminator for end of name

      while (*ep && *ep != 'F') ep++;  // look for (optional) flags

      if (indent > 0 && indent < 8 && strlen(np) > 0) {
        auto parent = load_stack[indent - 1];
        if (parent) {
          auto old = region_map.findByName(np);
          auto nw = temp_map.putRegion(np, parent->id, old ? old->id : 0);  // carry-over the current ID (if name already exists)
          if (nw) {
            nw->flags = old ? old->flags : (*ep == 'F' ? 0 : REGION_DENY_FLOOD);   // carry-over flags from curr

            load_stack[indent] = nw;  // keep pointers to parent regions, to resolve parent_id's
          }
        }
      }
      reply[0] = 0;
    }
    return;
  }

  while (*command == ' ') command++; // skip leading spaces

  if (strlen(command) > 4 && command[2] == '|') { // optional prefix (for companion radio CLI)
    memcpy(reply, command, 3);                    // reflect the prefix back
    reply += 3;
    command += 3;
  }

  // handle ACL related commands
  if (memcmp(command, "setperm ", 8) == 0) {   // format:  setperm {pubkey-hex} {permissions-int8}
    char* hex = &command[8];
    char* sp = strchr(hex, ' ');   // look for separator char
    if (sp == NULL) {
      strcpy(reply, "Err - bad params");
    } else {
      *sp++ = 0;   // replace space with null terminator

      uint8_t pubkey[PUB_KEY_SIZE];
      int hex_len = min(sp - hex, PUB_KEY_SIZE*2);
      if (mesh::Utils::fromHex(pubkey, hex_len / 2, hex)) {
        uint8_t perms = atoi(sp);
        if (acl.applyPermissions(self_id, pubkey, hex_len / 2, perms)) {
          dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);   // trigger acl.save()
          strcpy(reply, "OK");
        } else {
          strcpy(reply, "Err - invalid params");
        }
      } else {
        strcpy(reply, "Err - bad pubkey");
      }
    }
  } else if (sender_timestamp == 0 && strcmp(command, "get acl") == 0) {
    Serial.println("ACL:");
    for (int i = 0; i < acl.getNumClients(); i++) {
      auto c = acl.getClientByIdx(i);
      if (c->permissions == 0) continue;  // skip deleted (or guest) entries

      Serial.printf("%02X ", c->permissions);
      mesh::Utils::printHex(Serial, c->id.pub_key, PUB_KEY_SIZE);
      Serial.printf("\n");
    }
    reply[0] = 0;
  } else if (memcmp(command, "discover.neighbors", 18) == 0) {
    const char* sub = command + 18;
    while (*sub == ' ') sub++;
    if (*sub != 0) {
      strcpy(reply, "Err - discover.neighbors has no options");
    } else {
      sendNodeDiscoverReq();
      strcpy(reply, "OK - Discover sent");
    }
  }
#ifdef LOON_FIRMWARE
  else if (strcmp(command, "loon") == 0) {
    snprintf(reply, 160, "ping public=%s test=%s; announce public=%s test=%s; daily=%02u:00 UTC%+d:%02d",
             loon_prefs.ping_public ? "on" : "off", loon_prefs.ping_test ? "on" : "off",
             loonModeName(loon_prefs.announce_public), loonModeName(loon_prefs.announce_test),
             loon_prefs.daily_hour, loon_prefs.timezone_minutes / 60, abs(loon_prefs.timezone_minutes % 60));
  } else if (strncmp(command, "loon.ping.", 10) == 0) {
    uint8_t* setting = NULL;
    const char* value = NULL;
    if (strncmp(command + 10, "public", 6) == 0 && (command[16] == 0 || command[16] == ' ')) {
      setting = &loon_prefs.ping_public; value = command + 16;
    } else if (strncmp(command + 10, "test", 4) == 0 && (command[14] == 0 || command[14] == ' ')) {
      setting = &loon_prefs.ping_test; value = command + 14;
    }
    if (!setting) strcpy(reply, "Err - use loon.ping.public|test on|off");
    else {
      while (*value == ' ') value++;
      if (!*value) snprintf(reply, 160, "%s", *setting ? "on" : "off");
      else if (!strcmp(value, "on") || !strcmp(value, "1")) { *setting = 1; saveLoonPrefs(); strcpy(reply, "OK"); }
      else if (!strcmp(value, "off") || !strcmp(value, "0")) { *setting = 0; saveLoonPrefs(); strcpy(reply, "OK"); }
      else strcpy(reply, "Err - use on|off");
    }
  } else if (strncmp(command, "loon.announce.public.message", 28) == 0 &&
             (command[28] == 0 || command[28] == ' ')) {
    const char* value = command + 28; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%s", loon_prefs.announcement_public_message[0] ? loon_prefs.announcement_public_message : "default");
    else if (!strcmp(value, "off") || !strcmp(value, "clear")) {
      loon_prefs.announcement_public_message[0] = 0; saveLoonPrefs(); strcpy(reply, "OK");
    } else if (strlen(value) >= sizeof(loon_prefs.announcement_public_message)) {
      strcpy(reply, "Err - message must be 140 characters or fewer");
    } else if (!loonAnnouncementIsSafe(value)) {
      strcpy(reply, "Err - message cannot begin with !");
    } else {
      StrHelper::strncpy(loon_prefs.announcement_public_message, value, sizeof(loon_prefs.announcement_public_message));
      saveLoonPrefs(); strcpy(reply, "OK");
    }
  } else if (strncmp(command, "loon.announce.test.message", 26) == 0 &&
             (command[26] == 0 || command[26] == ' ')) {
    const char* value = command + 26; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%s", loon_prefs.announcement_test_message[0] ? loon_prefs.announcement_test_message : "default");
    else if (!strcmp(value, "off") || !strcmp(value, "clear")) {
      loon_prefs.announcement_test_message[0] = 0; saveLoonPrefs(); strcpy(reply, "OK");
    } else if (strlen(value) >= sizeof(loon_prefs.announcement_test_message)) {
      strcpy(reply, "Err - message must be 140 characters or fewer");
    } else if (!loonAnnouncementIsSafe(value)) {
      strcpy(reply, "Err - message cannot begin with !");
    } else {
      StrHelper::strncpy(loon_prefs.announcement_test_message, value, sizeof(loon_prefs.announcement_test_message));
      saveLoonPrefs(); strcpy(reply, "OK");
    }
  } else if (strncmp(command, "loon.announce.", 14) == 0) {
    uint8_t* setting = NULL;
    const char* value = NULL;
    if (strncmp(command + 14, "public", 6) == 0 && (command[20] == 0 || command[20] == ' ')) {
      setting = &loon_prefs.announce_public; value = command + 20;
    } else if (strncmp(command + 14, "test", 4) == 0 && (command[18] == 0 || command[18] == ' ')) {
      setting = &loon_prefs.announce_test; value = command + 18;
    }
    if (!setting) strcpy(reply, "Err - use loon.announce.public|test off|hourly|daily");
    else {
      while (*value == ' ') value++;
      if (!*value) snprintf(reply, 160, "%s", loonModeName(*setting));
      else {
        int mode = !strcmp(value, "off") ? 0 : !strcmp(value, "hourly") ? 1 : !strcmp(value, "daily") ? 2 : -1;
        if (mode < 0) strcpy(reply, "Err - use off|hourly|daily");
        else { *setting = mode; saveLoonPrefs(); scheduleLoonAnnouncements(); strcpy(reply, "OK"); }
      }
    }
  } else if (strncmp(command, "loon.daily.hour", 15) == 0) {
    const char* value = command + 15; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%u", loon_prefs.daily_hour);
    else { int n = atoi(value); if (n < 0 || n > 23) strcpy(reply, "Err - 0..23");
      else { loon_prefs.daily_hour = n; saveLoonPrefs(); scheduleLoonAnnouncements(); strcpy(reply, "OK"); } }
  } else if (strncmp(command, "loon.timezone", 13) == 0) {
    const char* value = command + 13; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%d", loon_prefs.timezone_minutes);
    else { int n = atoi(value); if (n < -720 || n > 840) strcpy(reply, "Err - minutes -720..840");
      else { loon_prefs.timezone_minutes = n; saveLoonPrefs(); scheduleLoonAnnouncements(); strcpy(reply, "OK"); } }
  } else if (strncmp(command, "loon.busy.threshold", 19) == 0) {
    const char* value = command + 19; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%u", loon_prefs.busy_threshold);
    else { int n = atoi(value); if (n < 0 || n > 100) strcpy(reply, "Err - 0..100");
      else { loon_prefs.busy_threshold = n; saveLoonPrefs(); strcpy(reply, "OK"); } }
  }
#if defined(ESP32)
  else if (strcmp(command, "wifi.status") == 0) {
    if (!loon_webhook_prefs.wifi_ssid[0]) strcpy(reply, "wifi: off");
    else if (WiFi.status() == WL_CONNECTED) {
      String ip = WiFi.localIP().toString();
      snprintf(reply, 160, "wifi: ok ip=%s queue=%u", ip.c_str(), loon_webhook_count);
    } else snprintf(reply, 160, "wifi: err queue=%u", loon_webhook_count);
  } else if (strncmp(command, "wifi.ssid", 9) == 0 && (command[9] == 0 || command[9] == ' ')) {
    const char* value = command + 9; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%s", loon_webhook_prefs.wifi_ssid[0] ? loon_webhook_prefs.wifi_ssid : "empty");
    else if (strlen(value) >= sizeof(loon_webhook_prefs.wifi_ssid)) strcpy(reply, "Err - SSID too long");
    else {
      StrHelper::strncpy(loon_webhook_prefs.wifi_ssid, value, sizeof(loon_webhook_prefs.wifi_ssid));
      saveLoonWebhookPrefs(); initLoonWifi(); strcpy(reply, "OK");
    }
  } else if (strncmp(command, "wifi.pwd", 8) == 0 && (command[8] == 0 || command[8] == ' ')) {
    const char* value = command + 8; while (*value == ' ') value++;
    if (!*value) {
      if (loon_webhook_prefs.wifi_password[0])
        snprintf(reply, 160, "set (len=%u)", (unsigned)strlen(loon_webhook_prefs.wifi_password));
      else strcpy(reply, "empty");
    } else if (!strcmp(value, "clear")) {
      loon_webhook_prefs.wifi_password[0] = 0; saveLoonWebhookPrefs(); WiFi.disconnect(); strcpy(reply, "OK");
    } else if (strlen(value) >= sizeof(loon_webhook_prefs.wifi_password)) strcpy(reply, "Err - password too long");
    else {
      StrHelper::strncpy(loon_webhook_prefs.wifi_password, value, sizeof(loon_webhook_prefs.wifi_password));
      saveLoonWebhookPrefs(); initLoonWifi(); strcpy(reply, "OK");
    }
  } else if (strncmp(command, "wifi.webhook.channel", 20) == 0 &&
             (command[20] == 0 || command[20] == ' ')) {
    const char* value = command + 20; while (*value == ' ') value++;
    if (!*value) snprintf(reply, 160, "%s", loon_webhook_prefs.channel_name);
    else if (!strcasecmp(value, "all")) strcpy(reply, "Err - one channel only");
    else {
      const char* normalized = !strcasecmp(value, "public") ? "Public" :
                               (!strcasecmp(value, "test") ? "#test" : value);
      if (strlen(normalized) >= sizeof(loon_webhook_prefs.channel_name)) strcpy(reply, "Err - channel too long");
      else {
        StrHelper::strncpy(loon_webhook_prefs.channel_name, normalized, sizeof(loon_webhook_prefs.channel_name));
        loon_webhook_head = loon_webhook_tail = loon_webhook_count = 0;
        saveLoonWebhookPrefs(); initLoonWebhookChannel(); strcpy(reply, "OK");
      }
    }
  } else if (strncmp(command, "wifi.webhook", 12) == 0 &&
             (command[12] == 0 || command[12] == ' ')) {
    const char* value = command + 12; while (*value == ' ') value++;
    if (!*value) {
      if (loon_webhook_prefs.discord_webhook_url[0])
        snprintf(reply, 160, "set (len=%u) channel=%s", (unsigned)strlen(loon_webhook_prefs.discord_webhook_url),
                 loon_webhook_prefs.channel_name);
      else strcpy(reply, "empty");
    } else if (!strcmp(value, "test")) {
      if (!loon_webhook_prefs.discord_webhook_url[0]) strcpy(reply, "Err - webhook not set");
      else { queueLoonWebhook("WebhookTest", "test message"); strcpy(reply, "OK - queued"); }
    } else if (!strcmp(value, "clear")) {
      loon_webhook_prefs.discord_webhook_url[0] = 0;
      loon_webhook_head = loon_webhook_tail = loon_webhook_count = 0;
      saveLoonWebhookPrefs(); strcpy(reply, "OK");
    } else if (strncmp(value, "https://", 8) != 0) {
      strcpy(reply, "Err - URL must begin https://");
    } else if (strlen(value) >= sizeof(loon_webhook_prefs.discord_webhook_url)) strcpy(reply, "Err - webhook URL too long");
    else {
      StrHelper::strncpy(loon_webhook_prefs.discord_webhook_url, value,
                         sizeof(loon_webhook_prefs.discord_webhook_url));
      loon_webhook_head = loon_webhook_tail = loon_webhook_count = 0;
      saveLoonWebhookPrefs(); strcpy(reply, "OK");
    }
  } else if (strcmp(command, "wifi.connect") == 0) {
    initLoonWifi(); strcpy(reply, "OK");
  }
#endif
#endif
  else{
    _cli.handleCommand(sender_timestamp, command, reply);  // common CLI commands
  }
}

void MyMesh::loop() {
#ifdef WITH_BRIDGE
  bridge.loop();
#endif

  mesh::Mesh::loop();

#ifdef LOON_FIRMWARE
  calcLoonBusyPercent();
#if defined(ESP32)
  pumpLoonWebhook();
#endif
  if (loon_next_public_announcement && millisHasNowPassed(loon_next_public_announcement)) {
    if (loon_prefs.announce_public && getRTCClock()->getCurrentTime() >= LOON_VALID_CLOCK)
      sendLoonAnnouncement(loon_public_channel);
    loon_next_public_announcement = nextLoonAnnouncement(loon_prefs.announce_public);
  }
  if (loon_next_test_announcement && millisHasNowPassed(loon_next_test_announcement)) {
    if (loon_prefs.announce_test && getRTCClock()->getCurrentTime() >= LOON_VALID_CLOCK)
      sendLoonAnnouncement(loon_test_channel);
    loon_next_test_announcement = nextLoonAnnouncement(loon_prefs.announce_test);
  }
  if (loon_rps3_remaining && loon_next_rps3_throw && millisHasNowPassed(loon_next_rps3_throw)) {
    static const char* choices[] = {"🪨", "📄", "✂️"};
    char result[LOON_MAX_ANNOUNCEMENT_TEXT + 1];
    uint8_t round = 4 - loon_rps3_remaining;
    snprintf(result, sizeof(result), "%u/3 %s", round,
             choices[getRNG()->nextInt(0, 3)]);
    sendLoonReply(loon_test_channel, result);
    loon_rps3_remaining--;
    loon_next_rps3_throw = loon_rps3_remaining ? futureMillis(10000UL) : 0;
  }
#endif

  if (next_flood_advert && millisHasNowPassed(next_flood_advert)) {
    mesh::Packet *pkt = createSelfAdvert();
    uint32_t delay_millis = 0;
    if (pkt) sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);

    updateFloodAdvertTimer(); // schedule next flood advert
    updateAdvertTimer();      // also schedule local advert (so they don't overlap)
  } else if (next_local_advert && millisHasNowPassed(next_local_advert)) {
    mesh::Packet *pkt = createSelfAdvert();
    if (pkt) sendZeroHop(pkt);

    updateAdvertTimer(); // schedule next local advert
  }

  if (set_radio_at && millisHasNowPassed(set_radio_at)) { // apply pending (temporary) radio params
    set_radio_at = 0;                                     // clear timer
    radio_driver.setParams(pending_freq, pending_bw, pending_sf, pending_cr);
    MESH_DEBUG_PRINTLN("Temp radio params");
  }

  if (revert_radio_at && millisHasNowPassed(revert_radio_at)) { // revert radio params to orig
    revert_radio_at = 0;                                        // clear timer
    radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
    MESH_DEBUG_PRINTLN("Radio params restored");
  }

  // is pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    acl.save(_fs);
    dirty_contacts_expiry = 0;
  }

  // update uptime
  uint32_t now = millis();
  uptime_millis += now - last_millis;
  last_millis = now;
}

// To check if there is pending work
bool MyMesh::hasPendingWork() const {
#if defined(WITH_BRIDGE)
  if (bridge.isRunning()) return true;  // bridge needs WiFi radio, can't sleep
#endif
  return _mgr->getOutboundTotal() > 0;
}
