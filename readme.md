# Chess Game with C++

I built a complete functional chess game using baord state and legal move generation engine with c++, CMake, and Dear ImGui, integrated into the prebuilt boardgame building class system by Professor Graeme Devine.I will write an AI using the negamax algorithm in the next steps so I wanted to ensure good performance. That is why I used a 64-bit integer bitboard for the entire board and move generation logic.


## System & Tools
* **OS:** Windows
* **Language:** C++
* **Dependencies:** Dear ImGui, CMake

## Game Engine Features
* **Gameplay Rules:** I implemented all chess game rules, however the pawn promotion automatically transforms into a queen rather than giving an option to the player. (even though minor this decision will help with the speed of the Negamax algorithm)
* **State Management:** I am using FEN (Forsyth-Edwards Notation) parsing. It loads board states, helps with saving and restoring games (not implemented yet), and help  track castling rights, implement en passant squares without having to slow down the game.

### Bitboard Architecture

* chess is not a simple game and it has way too many possible games. So iterating through 2d arrays to calculate attacks or discovering checks wasn't really an option due to speed concerns. That is why, I implemented bitboards to represent the occupancy of the board and individual piece types.
* I am utilizing bitwise shift operations to calculate moves using pre-calculated attacks.

### Magic Bitboards for Sliding Pieces
* for sligins pieces such as the rook, bishop or the queen, using the standard bit shifting is not really possible because their movement can be blocked by other pieces. 
* For its efficieny I used **Magic Bitboards**. This uses a precomputed hash table (which was given to us by Porfessor Graeme Devine). The hash table helps to instantly look up the exact attack rays of a sliding piece based on the current board, which gives us O(1) efficiency.

### Move Verification (Shannon's Algorithm)
To make sure that my game was calculating all possible moves,I implemented Shannon's algorithm (also known as a Perft or Performance Test) to a depth of 3. This made sure that I my chess board was considering all moves. Though depth 3 is a very low depth it should be enough to showcase all edge cases.