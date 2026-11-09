// Coin and credit counter.

#pragma once

#include <algorithm>
#include <string>

#include "core/sound.h"

namespace piu {

// Coin counter of the original machine.
class CoinBox {
 public:
  void configure(int coins_per_credit, bool free_play) {
    coins_per_credit_ = std::clamp(coins_per_credit, 1, 10);
    free_play_ = free_play;
  }

  void insert() { ++coins_; }
  // The sound of the next coin; the one that completes a credit differs.
  int next_coin_sound() const { return coins_ % coins_per_credit_ == coins_per_credit_ - 1 ? sfx::kCredit : sfx::kCoin; }
  // Free credits, worth the coins of a credit each.
  void add_credits(int credits) { coins_ += credits * coins_per_credit_; }
  // The count lives only in RAM, outside the EEPROM: turning the machine off loses it.
  void reset() { coins_ = 0; }

  bool can_start(int credits_needed) const {
    return free_play_ || coins_ >= std::max(1, credits_needed) * coins_per_credit_;
  }

  // The title screen wants a full credit even on free play.
  bool can_afford_one() const { return coins_ >= coins_per_credit_; }

  // Charges one credit, unless the machine is on free play.
  bool consume() {
    if (free_play_) return true;
    if (coins_ < coins_per_credit_) return false;
    coins_ -= coins_per_credit_;
    return true;
  }

  bool free_play() const { return free_play_; }
  int coins() const { return coins_; }
  int coins_per_credit() const { return coins_per_credit_; }
  int credits() const { return coins_ / coins_per_credit_; }

  // "3" or, with coins pending toward the next credit, "3(2/5)".
  std::string text() const {
    const int pending = coins_ % coins_per_credit_;
    std::string out = std::to_string(credits());
    if (coins_per_credit_ > 1 && pending != 0) {
      out += "(" + std::to_string(pending) + "/" + std::to_string(coins_per_credit_) + ")";
    }
    return out;
  }

 private:
  int coins_ = 0;
  int coins_per_credit_ = 5;
  bool free_play_ = false;
};

}  // namespace piu
