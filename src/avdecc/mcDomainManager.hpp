/*
* Copyright 2017-2018, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be usefu_state,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <la/avdecc/controller/avdeccController.hpp>
#include <memory>
#include <optional>
#include <QObject>
#include <QMap>

#include "avdecc/controllerManager.hpp"

namespace avdecc
{
namespace mediaClock
{
enum class CommandExecutionError
{
	NoError,
	LockedByOther,
	AcquiredByOther,
	EntityError,
	CommandFailure,
	NetworkIssue,
	Timeout,
	NoMediaClockOutputAvailable,
	NoMediaClockInputAvailable,
};

struct CommandErrorInfo
{
	CommandExecutionError errorType;
	std::optional<avdecc::ControllerManager::AcmpCommandType> commandTypeAcmp = std::nullopt;
	std::optional<avdecc::ControllerManager::AecpCommandType> commandTypeAecp = std::nullopt;
};

using CommandExecutionErrors = std::unordered_multimap<la::avdecc::UniqueIdentifier, CommandErrorInfo, la::avdecc::UniqueIdentifier::hash>;

// **************************************************************
// class AsyncParallelCommandSet
// **************************************************************
/**
* @brief    Holds n function pointers of the form std::function<bool(void)> that can be executed
			using the exec() method.
* [@author  Marius Erlen]
* [@date    2018-11-22]
*/
class AsyncParallelCommandSet : public QObject
{
	Q_OBJECT

public:
	using AsyncCommand = std::function<bool(AsyncParallelCommandSet* parentCommandSet, int commandIndex)>;

	static CommandExecutionError controlStatusToCommandError(la::avdecc::entity::ControllerEntity::ControlStatus status);
	static CommandExecutionError aemCommandStatusToCommandError(la::avdecc::entity::ControllerEntity::AemCommandStatus status);

public:
	AsyncParallelCommandSet();
	AsyncParallelCommandSet(AsyncCommand command);
	AsyncParallelCommandSet(std::vector<AsyncCommand> commands);

	void append(AsyncCommand command);
	void append(std::vector<AsyncCommand> commands);

	void addErrorInfo(la::avdecc::UniqueIdentifier entityId, CommandExecutionError error, avdecc::ControllerManager::AcmpCommandType commandType);
	void addErrorInfo(la::avdecc::UniqueIdentifier entityId, CommandExecutionError error, avdecc::ControllerManager::AecpCommandType commandType);
	void addErrorInfo(la::avdecc::UniqueIdentifier entityId, CommandExecutionError error);

	size_t parallelCommandCount() const noexcept;
	void exec() noexcept;

	void invokeCommandCompleted(int commandIndex, bool error) noexcept;

	Q_SIGNAL void commandSetCompleted(CommandExecutionErrors errors); // emitted after all commands in this command set were executed.

private:
	CommandExecutionErrors _errors;
	std::vector<AsyncCommand> _commands;
	int _commandCompletionCounter = 0;
	bool _errorOccured = false;
};

// **************************************************************
// class SequentialAsyncCommandExecuter
// **************************************************************
/**
* @brief    Executes a list of commands that is set by calling setCommandChain().
*			The command chain can be started with the start() method.
*			The commands in that list are executed sequentially.
*			All commands inside a AsyncParallelCommandSet container are executed simultaneously.
*			Once all commands were executed the completed signal is invoked.
*			The error parameter is true if at least one of the commands in the chain failed to be executed.
*
*			!!! This solution will be replaced by a more general and extensive state machine implementation in the future. !!!
* [@author  Marius Erlen]
* [@date    2018-11-22]
*/
class SequentialAsyncCommandExecuter : public QObject
{
	Q_OBJECT
public:
	SequentialAsyncCommandExecuter();

	void setCommandChain(std::vector<AsyncParallelCommandSet*> commands);

	void start();

	Q_SIGNAL void progressUpdate(int completedCommands, int totalCommands);
	Q_SIGNAL void completed(CommandExecutionErrors errors);

private:
	CommandExecutionErrors _errors;
	std::vector<AsyncParallelCommandSet*> _commands;
	int _currentCommandSet = 0;
	int _totalCommandCount; // includes parallel sub commands
	int _completedCommandCount; // includes parallel sub commands
};

using DomainIndex = std::uint64_t; /** A virtual index attributed to each domain */

enum class McDeterminationError
{
	NoError,
	NotSupported,
	Recursive,
	StreamNotConnected,
	ExternalClockSource,
	UnknownEntity,
};

// **************************************************************
// class MCDomain
// **************************************************************
/**
* @brief    Data container for domain data.
* [@author  Marius Erlen]
* [@date    2018-10-14]
*/
class MCDomain
{
public:
	// Constructors
	MCDomain(DomainIndex const index, la::avdecc::UniqueIdentifier const mediaClockMaster = la::avdecc::UniqueIdentifier::getUninitializedUniqueIdentifier(), la::avdecc::entity::model::SamplingRate const samplingRate = la::avdecc::entity::model::SamplingRate::getNullSamplingRate()) noexcept;

	// Getters
	DomainIndex getDomainIndex() const noexcept;
	la::avdecc::UniqueIdentifier getMediaClockDomainMaster() const noexcept;
	la::avdecc::entity::model::SamplingRate getDomainSamplingRate() const noexcept;

	// Setters
	void setDomainIndex(DomainIndex index) noexcept;
	void setMediaClockDomainMaster(la::avdecc::UniqueIdentifier const entityId) noexcept;
	void setDomainSamplingRate(la::avdecc::entity::model::SamplingRate const samplingRate) noexcept;

	// Other methods
	QString getDisplayName() const noexcept;

	// Defaulted compiler auto-generated methods
	MCDomain(MCDomain const&) = default;
	MCDomain(MCDomain&&) = default;
	MCDomain& operator=(MCDomain const&) = default;
	MCDomain& operator=(MCDomain&&) = default;

private:
	DomainIndex _domainIndex{ 0u }; /** Index of the domain */
	la::avdecc::UniqueIdentifier _mediaClockMasterId{}; /** ID of the clock master for the domain */
	la::avdecc::entity::model::SamplingRate _samplingRate{ la::avdecc::entity::model::SamplingRate::getNullSamplingRate() }; /** Sampling rate for the whole domain */
};

// **************************************************************
// class MCEntityDomainMapping
// **************************************************************
/**
* @brief    Data container for mc domains and assigned entities.
* [@author  Marius Erlen]
* [@date    2018-10-14]
*/
class MCEntityDomainMapping
{
public:
	using Mappings = std::unordered_map<la::avdecc::UniqueIdentifier, std::vector<DomainIndex>, la::avdecc::UniqueIdentifier::hash>;
	using Domains = std::unordered_map<DomainIndex, MCDomain>;
	using Errors = std::unordered_map<la::avdecc::UniqueIdentifier, McDeterminationError, la::avdecc::UniqueIdentifier::hash>;

	MCEntityDomainMapping() noexcept;
	MCEntityDomainMapping(Mappings&& mappings, Domains&& domains, Errors&& errors) noexcept;

	std::optional<DomainIndex> const findDomainIndexByMasterEntityId(la::avdecc::UniqueIdentifier mediaClockMasterId) noexcept;

	Mappings& getEntityMediaClockMasterMappings() noexcept;
	Domains& getMediaClockDomains() noexcept;
	Errors& getEntityMcErrors() noexcept;

	// Defaulted compiler auto-generated methods
	MCEntityDomainMapping(MCEntityDomainMapping const&) = default;
	MCEntityDomainMapping(MCEntityDomainMapping&&) = default;
	MCEntityDomainMapping& operator=(MCEntityDomainMapping const&) = default;
	MCEntityDomainMapping& operator=(MCEntityDomainMapping&&) = default;

private:
	Mappings _entityMediaClockMasterMappings{};
	Domains _mediaClockDomains{};
	Errors _entityMcErrors{};
};

struct ApplyInfo
{
	CommandExecutionErrors entityApplyErrors;
};

// **************************************************************
// class MCDomainManager
// **************************************************************
/**
* @brief    This class provides helper methods to determine and track
*					media clock connections.
* [@author  Marius Erlen]
* [@date    2018-10-04]
*/
class MCDomainManager : public QObject
{
	Q_OBJECT

public:
	static MCDomainManager& getInstance() noexcept;

	/* media clock management helper functions */
	virtual std::pair<la::avdecc::UniqueIdentifier, McDeterminationError> getMediaClockMaster(la::avdecc::UniqueIdentifier const entityId) noexcept = 0;
	virtual MCEntityDomainMapping createMediaClockDomainModel() noexcept = 0;
	virtual void applyMediaClockDomainModel(MCEntityDomainMapping const& domains) noexcept = 0;
	virtual bool checkGPTPInSync(la::avdecc::UniqueIdentifier entityId) noexcept = 0;
	virtual bool isMediaClockDomainManageable(la::avdecc::UniqueIdentifier const& entityId) noexcept = 0;

	Q_SIGNAL void mediaClockConnectionsUpdate(std::vector<la::avdecc::UniqueIdentifier> entityIds);
	Q_SIGNAL void mcMasterNameChanged(std::vector<la::avdecc::UniqueIdentifier> entityIds);

	Q_SIGNAL void applyMediaClockDomainModelProgressUpdate(int progressPercentage);
	Q_SIGNAL void applyMediaClockDomainModelFinished(ApplyInfo);
};

constexpr bool operator!(McDeterminationError const error)
{
	return error == McDeterminationError::NoError;
}

} // namespace mediaClock
} // namespace avdecc
