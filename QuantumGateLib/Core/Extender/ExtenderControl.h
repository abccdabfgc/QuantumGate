// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#pragma once

#include "..\..\Concurrency\Queue.h"
#include "..\..\Concurrency\ThreadSafe.h"
#include "..\..\Concurrency\ThreadPool.h"
#include "..\Peer\PeerEvent.h"
#include "Extender.h"
#include "ExtenderModule.h"

namespace QuantumGate::Implementation::Core::Extender
{
	class Manager;

	class Control final
	{
		struct Peer final
		{
			enum class Status { Unknown, Connected, Disconnected };

			Peer() = delete;

			Peer(const UInt64 thpkey, std::atomic<Size>& peercount, Status status) noexcept :
				Status(status), ThreadPoolKey(thpkey), ThreadPoolPeerCount(peercount)
			{
				++ThreadPoolPeerCount;
			}

			Peer(const Peer&) = delete;
			Peer(Peer&&) noexcept = default;

			~Peer()
			{
				--ThreadPoolPeerCount;
			}

			Peer& operator=(const Peer&) = delete;
			Peer& operator=(Peer&&) noexcept = default;

			Status Status{ Status::Unknown };
			Concurrency::Queue<Core::Peer::Event> EventQueue;
			Concurrency::Queue<Core::Peer::Event> MessageQueue;

			bool IsInQueue{ false };
			const UInt64 ThreadPoolKey{ 0 };
			std::atomic<Size>& ThreadPoolPeerCount;
		};

		using Peer_ThS = Concurrency::ThreadSafe<Peer, Concurrency::SpinMutex>;

		using PeerMap = Containers::UnorderedMap<PeerLUID, std::shared_ptr<Peer_ThS>>;

		using Queue = Concurrency::Queue<std::shared_ptr<Peer_ThS>>;
		using Queue_ThS = Concurrency::ThreadSafe<Queue, Concurrency::SpinMutex>;

		struct ThreadPoolData final
		{
			Queue_ThS Queue;
			std::atomic<Size> PeerCount{ 0 };
		};

		using ThreadPool = Concurrency::ThreadPool<ThreadPoolData>;
		using ThreadPoolMap = Containers::UnorderedMap<UInt64, std::unique_ptr<ThreadPool>>;

	public:
		enum class Status { Startup, Running, Shutdown, Stopped };

		Control() = delete;
		Control(Manager& mgr, const std::shared_ptr<QuantumGate::API::Extender>& extender,
				const ExtenderModuleID moduleid) noexcept;
		Control(const Control&) = delete;
		Control(Control&&) noexcept = default;
		~Control() = default;
		Control& operator=(const Control&) = delete;
		Control& operator=(Control&&) noexcept = default;

		void SetStatus(const Status status) noexcept;
		inline Status GetStatus() const noexcept { return m_Status; }
		inline SteadyTime GetSteadyTimeRemoved() const noexcept { return m_SteadyTimeRemoved; }

		inline bool HasExtender() const noexcept { return (m_Extender != nullptr); }

		[[nodiscard]] inline bool IsSameExtender(const std::shared_ptr<QuantumGate::API::Extender>& extender,
												 const ExtenderModuleID moduleid) const noexcept
		{
			return (m_Extender.get() == extender.get() && m_ExtenderModuleID == moduleid);
		}

		inline void ReleaseExtender() noexcept { m_Extender.reset(); }

		inline const Extender& GetExtender() const noexcept
		{
			assert(m_Extender != nullptr && m_Extender->m_Extender != nullptr);
			return *m_Extender->m_Extender;
		}

		inline Extender& GetExtender() noexcept
		{
			return const_cast<Extender&>(const_cast<const Control*>(this)->GetExtender());
		}

		inline const std::shared_ptr<QuantumGate::API::Extender>& GetAPIExtender() const noexcept { return m_Extender; }

		inline String GetExtenderName() const noexcept { return GetExtenderName(GetExtender()); }

		static inline String GetExtenderName(const Extender& extender) noexcept
		{
			return Util::FormatString(L"'%s' (UUID: %s)", extender.GetName().c_str(),
									  extender.GetUUID().GetString().c_str());
		}

		[[nodiscard]] bool AddPeerEvent(Core::Peer::Event&& event) noexcept;

		[[nodiscard]] bool StartupExtenderThreadPools() noexcept;
		void ShutdownExtenderThreadPools() noexcept;

	private:
		void PreStartupExtenderThreadPools() noexcept;
		void ResetState() noexcept;

		ThreadPool::ThreadCallbackResult WorkerThreadProcessor(ThreadPoolData& thpdata,
															   const Concurrency::EventCondition& shutdown_event);

	private:
		Manager& m_ExtenderManager;

		Status m_Status{ Status::Stopped };
		std::shared_ptr<QuantumGate::API::Extender> m_Extender;
		ExtenderModuleID m_ExtenderModuleID{ 0 };
		SteadyTime m_SteadyTimeAdded;
		SteadyTime m_SteadyTimeRemoved;

		PeerMap m_Peers;
		ThreadPoolMap m_ThreadPools;
	};

	using Control_ThS = Concurrency::ThreadSafe<Control, std::shared_mutex>;
}