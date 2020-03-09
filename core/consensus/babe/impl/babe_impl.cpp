/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/babe/impl/babe_impl.hpp"

#include <sr25519/sr25519.h>
#include <boost/assert.hpp>
#include <boost/range/join.hpp>

#include "common/buffer.hpp"
#include "consensus/babe/babe_error.hpp"
#include "consensus/babe/impl/"
#include "consensus/babe/impl/babe_digests_util.hpp"
#include "consensus/babe/types/babe_block_header.hpp"
#include "consensus/babe/types/seal.hpp"
#include "network/types/block_announce.hpp"
#include "primitives/inherent_data.hpp"
#include "scale/scale.hpp"

namespace kagome::consensus {
  BabeImpl::BabeImpl(std::shared_ptr<BabeLottery> lottery,
                     std::shared_ptr<BabeSynchronizer> babe_synchronizer,
                     std::shared_ptr<BabeBlockValidator> block_validator,
                     std::shared_ptr<EpochStorage> epoch_storage,
                     std::shared_ptr<runtime::Core> core,
                     std::shared_ptr<authorship::Proposer> proposer,
                     std::shared_ptr<blockchain::BlockTree> block_tree,
                     std::shared_ptr<network::BabeGossiper> gossiper,
                     crypto::SR25519Keypair keypair,
                     primitives::AuthorityIndex authority_index,
                     std::shared_ptr<clock::SystemClock> clock,
                     std::shared_ptr<crypto::Hasher> hasher,
                     std::unique_ptr<clock::Timer> timer,
                     libp2p::event::Bus &event_bus)
      : lottery_{std::move(lottery)},
        babe_synchronizer_{std::move(babe_synchronizer)},
        block_validator_{std::move(block_validator)},
        epoch_storage_{std::move(epoch_storage)},
        core_{std::move(core)},
        proposer_{std::move(proposer)},
        block_tree_{std::move(block_tree)},
        gossiper_{std::move(gossiper)},
        keypair_{keypair},
        authority_index_{authority_index},
        clock_{std::move(clock)},
        hasher_{std::move(hasher)},
        timer_{std::move(timer)},
        event_bus_{event_bus},
        error_channel_{event_bus_.getChannel<event::BabeErrorChannel>()},
        log_{common::createLogger("BABE")} {
    BOOST_ASSERT(lottery_);
    BOOST_ASSERT(proposer_);
    BOOST_ASSERT(block_tree_);
    BOOST_ASSERT(gossiper_);
    BOOST_ASSERT(clock_);
    BOOST_ASSERT(hasher_);
  }

  void BabeImpl::runEpoch(Epoch epoch,
                          BabeTimePoint starting_slot_finish_time) {
    BOOST_ASSERT(!epoch.authorities.empty());
    log_->debug("starting an epoch with index {}. Session key: {}",
                epoch.epoch_index,
                keypair_.public_key.toHex());

    current_epoch_ = std::move(epoch);
    current_slot_ = current_epoch_.start_slot;

    auto threshold = slots_leadership_ = lottery_->slotsLeadership(
        current_epoch_.randomness, genesis_configuration_., keypair_);
    next_slot_finish_time_ = starting_slot_finish_time;

    runSlot();
  }

  BabeMeta BabeImpl::getBabeMeta() const {
    return BabeMeta{current_epoch_,
                    current_slot_,
                    slots_leadership_,
                    next_slot_finish_time_};
  }

  void BabeImpl::runSlot() {
    using std::chrono::operator""ms;
    static constexpr auto kMaxLatency = 5000ms;

    if (current_slot_ % genesis_configuration_.epoch_length == 0) {
      // end of the epoch
      return finishEpoch();
    }
    log_->info("starting a slot {} in epoch {}",
               current_slot_,
               current_epoch_.epoch_index);

    // check that we are really in the middle of the slot, as expected; we can
    // cooperate with a relatively little (kMaxLatency) latency, as our node
    // will be able to retrieve
    auto now = clock_->now();
    if (now > next_slot_finish_time_
        && (now - next_slot_finish_time_ > kMaxLatency)) {
      // we are too far behind; after skipping some slots (but not epochs)
      // control will be returned to this method

      // TODO
      return synchronizeBlocks();
    }

    // everything is OK: wait for the end of the slot
    timer_->expiresAt(next_slot_finish_time_);
    timer_->asyncWait([this](auto &&ec) {
      if (ec) {
        log_->error("error happened while waiting on the timer: {}",
                    ec.message());
        return error_channel_.publish(BabeError::TIMER_ERROR);
      }
      finishSlot();
    });
  }

  void BabeImpl::finishSlot() {
    auto slot_leadership = slots_leadership_[current_slot_];
    if (slot_leadership) {
      log_->debug("Peer {} is leader", authority_index_.index);
      processSlotLeadership(*slot_leadership);
    }

    ++current_slot_;
    next_slot_finish_time_ += genesis_configuration_.slot_duration;
    log_->debug("Slot {} in epoch {} has finished",
                current_slot_,
                current_epoch_.epoch_index);
    runSlot();
  }

  outcome::result<primitives::PreRuntime> BabeImpl::babePreDigest(
      const crypto::VRFOutput &output) const {
    BabeBlockHeader babe_header{current_slot_, output, authority_index_};
    auto encoded_header_res = scale::encode(babe_header);
    if (!encoded_header_res) {
      log_->error("cannot encode BabeBlockHeader: {}",
                  encoded_header_res.error().message());
      return encoded_header_res.error();
    }
    common::Buffer encoded_header{encoded_header_res.value()};

    return primitives::PreRuntime{{primitives::kBabeEngineId, encoded_header}};
  }

  primitives::Seal BabeImpl::sealBlock(const primitives::Block &block) const {
    auto pre_seal_encoded_block = scale::encode(block.header).value();

    auto pre_seal_hash = hasher_->blake2b_256(pre_seal_encoded_block);

    Seal seal{};
    seal.signature.fill(0);
    sr25519_sign(seal.signature.data(),
                 keypair_.public_key.data(),
                 keypair_.secret_key.data(),
                 pre_seal_hash.data(),
                 decltype(pre_seal_hash)::size());
    auto encoded_seal = common::Buffer(scale::encode(seal).value());
    return primitives::Seal{{primitives::kBabeEngineId, encoded_seal}};
  }

  void BabeImpl::processSlotLeadership(const crypto::VRFOutput &output) {
    // build a block to be announced
    log_->info("Obtained slot leadership");

    primitives::InherentData inherent_data;
    auto epoch_secs = std::chrono::duration_cast<std::chrono::seconds>(
                          clock_->now().time_since_epoch())
                          .count();
    // identifiers are guaranteed to be correct, so use .value() directly
    auto put_res = inherent_data.putData<uint64_t>(kTimestampId, epoch_secs);
    if (!put_res) {
      return log_->error("cannot put an inherent data: {}",
                         put_res.error().message());
    }
    put_res = inherent_data.putData(kBabeSlotId, current_slot_);
    if (!put_res) {
      return log_->error("cannot put an inherent data: {}",
                         put_res.error().message());
    }

    auto &&[best_block_number, best_block_hash] = block_tree_->deepestLeaf();
    log_->debug("Babe builds block on top of block with number {} and hash {}",
                best_block_number,
                best_block_hash);

    // calculate babe_pre_digest
    auto babe_pre_digest_res = babePreDigest(output);
    if (not babe_pre_digest_res) {
      return log_->error("cannot propose a block: {}",
                         babe_pre_digest_res.error().message());
    }
    auto babe_pre_digest = babe_pre_digest_res.value();

    // create new block
    auto pre_seal_block_res =
        proposer_->propose(best_block_hash, inherent_data, {babe_pre_digest});
    if (!pre_seal_block_res) {
      return log_->error("cannot propose a block: {}",
                         pre_seal_block_res.error().message());
    }

    auto block = pre_seal_block_res.value();
    // seal the block
    auto seal = sealBlock(block);

    // add seal digest item
    block.header.digest.emplace_back(seal);

    // add block to the block tree
    block_tree_->addBlock(block);

    // finally, broadcast the sealed block
    gossiper_->blockAnnounce(network::BlockAnnounce{block.header});
    log_->debug("Announced block in slot: {}", current_slot_);
  }

  void BabeImpl::finishEpoch() {
    // TODO(akvinikym) PRE-291: validator update - how is it done?

    // TODO(akvinikym) PRE-291: compute new epoch duration
    // (BabeApi_slot_duration runtime entry)

    // TODO(akvinikym) PRE-291: compute new threshold
    // (BabeApi_slot_winning_threshold runtime entry)

    // compute new randomness
    const auto &next_epoch_digest_opt =
        epoch_storage_->getEpochDescriptor(++current_epoch_.epoch_index);
    if (not next_epoch_digest_opt) {
      log_->error("Next epoch digest does not exist");
      return;
    }

    current_epoch_.authorities = next_epoch_digest_opt->authorities;
    current_epoch_.randomness = next_epoch_digest_opt->randomness;

    log_->debug("Epoch {} has finished", current_epoch_.epoch_index);
    runEpoch(current_epoch_, next_slot_finish_time_);
  }

  void BabeImpl::processNextBlock(const primitives::Block &new_block) {
    if (auto apply_res = applyBlock(new_block); not apply_res) {
      log_->error("Could not apply block during synchronizing slots. Error: {}",
                  apply_res.error().message());
    }

    auto babe_digests_res = getBabeDigests(new_block.header);
    if (not babe_digests_res) {
      log_->error("Could not get digests: {}",
                  babe_digests_res.error().message());
    }

    auto [_, babe_header] = babe_digests_res.value();
    auto observed_slot = babe_header.slot_number;

    auto epoch_index = genesis_configuration_.epoch_length;

    // update authorities and randomnesss
    auto next_epoch_digest_res = getNextEpochDigest(new_block.header);
    if (not next_epoch_digest_res) {
      return;
    }
    epoch_storage_->addEpochDescriptor(epoch_index,
                                       next_epoch_digest_res.value());
  }

  Epoch BabeImpl::epochForChildOf(const primitives::BlockHash &parent_hash,
                                  primitives::BlockNumber parent_number,
                                  BabeSlotNumber slot_number) const {}

  void BabeImpl::synchronizeSlots(const primitives::Block &new_block) {
    static boost::optional<BabeSlotNumber> first_production_slot = boost::none;

    if (auto apply_res = applyBlock(new_block); not apply_res) {
      log_->error("Could not apply block during synchronizing slots. Error: {}",
                  apply_res.error().message());
    }

    auto babe_digests_res = getBabeDigests(new_block.header);
    if (not babe_digests_res) {
      log_->error("Could not get digests: {}",
                  babe_digests_res.error().message());
    }

    auto [_, babe_header] = babe_digests_res.value();
    auto observed_slot = babe_header.slot_number;

    if (not first_production_slot) {
      first_production_slot = observed_slot + kSlotTail;
    }

    // get the difference between observed slot and the one that we are trying
    // to launch
    auto diff = *first_production_slot - observed_slot;

    first_slot_times_.emplace_back(
        clock_->nowUint64() + diff * genesis_configuration_.slot_duration);
    if (observed_slot >= first_production_slot.value()) {
      current_state_ = BabeState::SYNCHRONIZED;

      // get median as here:
      // https://en.cppreference.com/w/cpp/algorithm/nth_element
      std::nth_element(first_slot_times_.begin(),
                       first_slot_times_.begin() + first_slot_times_.size() / 2,
                       first_slot_times_.end());
      auto first_slot_ending_time =
          first_slot_times_[first_slot_times_.size() / 2];

      Epoch epoch;
      epoch.epoch_index =
          *first_production_slot / genesis_configuration_.epoch_length;
      epoch.start_slot = *first_production_slot;
      epoch.epoch_duration = genesis_configuration_.epoch_length;

      // fill other epoch fields
      runEpoch(epoch, first_slot_ending_time);
    }
  }

  void BabeImpl::synchronizeBlocks(const primitives::Block &new_block,
                                   std::function<void()> next) {
    const auto &[last_number, last_hash] = block_tree_->getLastFinalized();
    BOOST_ASSERT(new_block.header.number <= last_number);
    babe_synchronizer_->request(
        new_block.header.parent_hash,
        last_hash,
        [self{shared_from_this()}, next{std::move(next)}, new_block](
            const std::vector<primitives::Block> &blocks) {
          for (const auto &block : boost::join(blocks, new_block)) {
            if (auto apply_res = self->applyBlock(block); not apply_res) {
              self->log_->error("Could not apply block: {}",
                                apply_res.error().message());
              return;
            }
          }
          next();
        });
  }

  outcome::result<void> BabeImpl::applyBlock(const primitives::Block &block) {
    // TODO(kamilsa): validate block
    // if (auto validate_res = block_validator_->validate(block))

    // apply block
    OUTCOME_TRY(core_->execute_block(block));
    OUTCOME_TRY(block_tree_->addBlock(block));

    return outcome::success();
  }

  void BabeImpl::onBlockAnnounce(const network::BlockAnnounce &announce) {
    switch (current_state_) {
      case BabeState::START:
        current_state_ = BabeState::CATCHING_UP;
        synchronizeBlocks(announce.block, [self{shared_from_this()}] {
          // all blocks were successfully applied, now we need to get slot time
          self->current_state_ = BabeState::NEED_SLOT_TIME;
        });
        break;
      case BabeState::NEED_SLOT_TIME:
        synchronizeSlots(announce.block);
        break;
      case BabeState::CATCHING_UP:
      case BabeState::SYNCHRONIZED:
        processNextBlock(announce.block);
        break;
    }
  }
}  // namespace kagome::consensus
