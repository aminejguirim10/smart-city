# 🏙️ Smart City

**Smart City** is a C-based network application built around three complementary console interfaces:

- a **server** that centralizes incidents and emergency messages
- a **citizen client** that lets users report incidents or send emergency messages
- an **administrator console** that allows the admin to read pending emergency messages

The system broadcasts incidents in real time to connected clients and keeps a history of previously recorded reports.

## ✨ Features

- Report 4 incident types: **ACCIDENT**, **FIRE**, **POWER OUTAGE**, and **BLOCKED ROAD**.
- Broadcast incidents immediately to all connected clients.
- Send incident history to every new client upon connection.
- Forward emergency messages to the administrator using a bounded queue.
- Notify the sender when an emergency message has been read by the administrator.
- Provide a simple and readable console interface for the three system roles.

## 🛠️ Technologies Used

- **Language:** C
- **Networking:** POSIX TCP sockets
- **Concurrency:** pthreads
- **Synchronization:** POSIX semaphores
- **Build Tooling:** GCC + GNU Make

## 📋 Prerequisites

This project targets a POSIX environment.

- GCC or Clang with POSIX support
- `make`
- Linux, macOS, or WSL recommended
- Multiple terminals to run the server, client, and admin side by side

## 🚀 Installation

1. Clone the repository:

```bash
git clone https://github.com/aminejguirim10/smart-city.git
```

2. Move into the project folder:

```bash
cd smart-city
```

3. Build the executables:

```bash
make
```

4. Start the server in one terminal:

```bash
./server
```

5. Start one or more clients in other terminals:

```bash
./client
```

6. Start the administrator console in another terminal:

```bash
./admin
```

If you are using Windows through a POSIX-compatible environment, adapt the executable names to match your compiler output.

## 🧠 How It Works

1. The server starts on port `8080`.
2. The client asks for a nickname and identifies itself as `CLIENT`.
3. On connection, the client receives the current incident history.
4. The citizen can report an incident or send an emergency message.
5. The server broadcasts incidents to all clients and manages a FIFO queue for emergency messages.
6. The administrator connects as `ADMIN` and reads emergency messages through the dedicated menu option.
7. When a message is read, the sender receives a read confirmation.

## 🗂️ Project Structure

```text
smart-city/
├── admin.c
├── client.c
├── incident.h
├── makefile
├── server.c
└── SmartCity_Presentation.pptx
```

- `server.c`: multi-threaded TCP server, client management, incident history, and emergency queue handling.
- `client.c`: citizen interface for reporting incidents and receiving real-time notifications.
- `admin.c`: administrator interface for reading pending emergency messages.
- `incident.h`: shared data structures, size constants, and acknowledgment codes.
- `makefile`: build, clean, and rebuild rules.
- `SmartCity_Presentation.pptx`: project presentation material.

## ⚙️ Important Settings

- The server listens on `127.0.0.1:8080` by default.
- The emergency message queue is limited to `10` messages.
- The incident history is limited to `100` entries.
- Emergency messages use the acknowledgments `MSG_ENVOYE`, `MSG_ATTENTE`, `MSG_PLEIN`, and `MSG_LU`.

## 🤝 Contributors

- Mohamed Amine Jguirim
- Fares Tekaya
- Wiem Omrani
- AbdelHakim Barbaria

## 🚶 Contributing

Contributions are welcome.

1. Fork the repository.
2. Create a dedicated branch.
3. Commit your changes.
4. Push your branch.
5. Open a pull request.
