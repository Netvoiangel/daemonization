#include "wolf_game.hpp"

#include <cmath>
#include <iostream>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <sstream>
#include <stdexcept>

#include "ipc_utils.hpp"
#include "logging.hpp"

namespace {

int readWolfNumber(int timeoutSec) {
  std::cout << "Введите число волка (1-100), у вас " << timeoutSec << " сек: " << std::flush;
  struct pollfd pfd {
    .fd = STDIN_FILENO,
    .events = POLLIN,
    .revents = 0,
  };
  int ms = timeoutSec * 1000;
  int pollRes = poll(&pfd, 1, ms);
  if (pollRes > 0 && (pfd.revents & POLLIN)) {
    std::string line;
    std::getline(std::cin, line);
    std::istringstream iss(line);
    int value = 0;
    if (iss >> value && value >= 1 && value <= 100) {
      return value;
    }
    logMessage(LogLevel::Warning, "Некорректный ввод, выбираю случайное число.");
  } else {
    logMessage(LogLevel::Info, "Таймаут ввода, волк выбирает случайное число.");
  }
  return randomBetween(1, 100);
}

std::string messageToString(const Message &msg) {
  std::ostringstream oss;
  oss << "type=" << static_cast<int>(msg.type) << ", round=" << msg.round
      << ", value=" << msg.value;
  return oss.str();
}

} // namespace

void runHostGame(std::unique_ptr<ConnectionEndpoint> conn, const HostConfig &config) {
  if (!conn) {
    throw std::runtime_error("Connection is null");
  }

  logMessage(LogLevel::Info,
             "Хост готов. PID " + std::to_string(getpid()) + ". Ожидаю сигнал от клиента.");
  installHandshakeHandler();
  if (!waitForHandshake(kHandshakeDefaultTimeoutSec)) {
    logMessage(LogLevel::Error, "Знакомство не удалось: сигнал не получен.");
    return;
  }
  logMessage(LogLevel::Info, "Сигнал получен, начинаем игру.");

  bool goatAlive = true;
  int consecutiveDeadRounds = 0;
  uint32_t round = 0;
  const double hideThreshold = 70.0 / static_cast<double>(config.goats);
  const double resurrectThreshold = 20.0 / static_cast<double>(config.goats);

  while (consecutiveDeadRounds < 2) {
    ++round;
    int wolfValue = readWolfNumber(config.wolfInputTimeoutSec);
    logMessage(LogLevel::Info,
               "Раунд " + std::to_string(round) + ": волк выбросил " + std::to_string(wolfValue));

    Message start{MessageType::RoundStart, round, goatAlive ? 1 : 0};
    if (!conn->send(start)) {
      logMessage(LogLevel::Error, "Не удалось отправить запрос раунда.");
      return;
    }

    Message goatMsg;
    if (!conn->receive(goatMsg, kSemaphoreTimeoutSec)) {
      logMessage(LogLevel::Error, "Не удалось получить число козлёнка (таймаут).");
      return;
    }
    if (goatMsg.type != MessageType::GoatNumber || goatMsg.round != round) {
      logMessage(LogLevel::Error, "Получено некорректное сообщение: " + messageToString(goatMsg));
      return;
    }

    const int goatValue = goatMsg.value;
    std::ostringstream roundLog;
    roundLog << "Козлёнок выбросил " << goatValue << ". ";
    bool hidden = false;
    if (goatAlive) {
      hidden = std::abs(wolfValue - goatValue) <= hideThreshold;
      if (hidden) {
        roundLog << "Козлёнок спрятался.";
      } else {
        goatAlive = false;
        roundLog << "Козлёнок пойман.";
      }
    } else {
      bool resurrect = std::abs(wolfValue - goatValue) <= resurrectThreshold;
      if (resurrect) {
        goatAlive = true;
        roundLog << "Козлёнок воскрес.";
      } else {
        roundLog << "Козлёнок остаётся мёртвым.";
      }
    }

    Message status{MessageType::Status, round, goatAlive ? 1 : 0};
    if (!conn->send(status)) {
      logMessage(LogLevel::Error, "Не удалось отправить статус козлёнка.");
      return;
    }

    if (goatAlive) {
      consecutiveDeadRounds = 0;
    } else {
      ++consecutiveDeadRounds;
    }
    roundLog << " Живых: " << (goatAlive ? 1 : 0) << ", мёртвых: " << (goatAlive ? 0 : 1)
             << ", спрятались: " << (hidden ? 1 : 0) << ", пойманы: " << (hidden ? 0 : 1);
    logMessage(LogLevel::Info, roundLog.str());
  }

  Message terminate{MessageType::Terminate, round, 0};
  conn->send(terminate);
  logMessage(LogLevel::Info, "Игра завершена: два раунда подряд козлёнок мёртв.");
}

void runClientGame(std::unique_ptr<ConnectionEndpoint> conn, const ClientConfig &config) {
  if (!conn) {
    throw std::runtime_error("Connection is null");
  }

  if (!sendHandshakeSignal(config.hostPid)) {
    throw std::runtime_error("Не удалось отправить сигнал хосту.");
  }
  logMessage(LogLevel::Info, "SIGUSR1 отправлен процессу " + std::to_string(config.hostPid));

  bool goatAlive = true;
  while (true) {
    Message request;
    if (!conn->receive(request, kSemaphoreTimeoutSec)) {
      logMessage(LogLevel::Error, "Ожидание раунда истекло.");
      return;
    }

    if (request.type == MessageType::Terminate) {
      logMessage(LogLevel::Info, "Получен сигнал завершения от хоста.");
      return;
    }

    if (request.type != MessageType::RoundStart) {
      logMessage(LogLevel::Warning,
                 "Получен неожиданный тип сообщения: " + messageToString(request));
      continue;
    }

    goatAlive = request.value != 0;
    int goatValue = goatAlive ? randomBetween(1, 100) : randomBetween(1, 50);
    Message response{MessageType::GoatNumber, request.round, goatValue};
    if (!conn->send(response)) {
      logMessage(LogLevel::Error, "Не удалось отправить число козлёнка.");
      return;
    }

    Message status;
    if (!conn->receive(status, kSemaphoreTimeoutSec)) {
      logMessage(LogLevel::Error, "Не удалось получить статус козлёнка.");
      return;
    }
    if (status.type != MessageType::Status || status.round != request.round) {
      logMessage(LogLevel::Warning,
                 "Получен неожиданный статус: " + messageToString(status));
      continue;
    }
    goatAlive = status.value != 0;
    logMessage(LogLevel::Info,
               "Раунд " + std::to_string(status.round) + ": моё число " +
                   std::to_string(goatValue) + ", статус: " + (goatAlive ? "жив" : "мёртв"));
  }
}


