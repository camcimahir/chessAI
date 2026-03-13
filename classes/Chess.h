#pragma once


#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include <cassert>
#include <cstdint>
#include <iostream>

constexpr int pieceSize = 80;

using BitBoard = BitboardElement;

enum BitBoardPieceIndex {
    WHITE_PAWNS,
    WHITE_KNIGHTS,
    WHITE_BISHOPS,
    WHITE_ROOKS,
    WHITE_QUEENS,
    WHITE_KINGS,
    BLACK_PAWNS,
    BLACK_KNIGHTS,
    BLACK_BISHOPS,
    BLACK_ROOKS,
    BLACK_QUEENS,
    BLACK_KINGS,
    WHITE_ALL_PIECES,
    BLACK_ALL_PIECES,
    OCCUPANCY,
    e_numBitBoards
};


constexpr int MAX_DEPTH = 64;
struct GameState {
    char state[64];
    int stackLevel;
    int srcSquares[MAX_DEPTH];
    int dstSquares[MAX_DEPTH];
    char saveMoves[MAX_DEPTH];
    GameState() : stackLevel(0) {
        for (int i = 0; i < 64; ++i) state[i] = '0';
    }
    void pushMove(BitMove& move) {
        if (stackLevel >= MAX_DEPTH) {
            std::cerr << "ERROR: pushMove stack overflow! stackLevel=" << stackLevel 
                      << " MAX_DEPTH=" << MAX_DEPTH << std::endl;
            assert(false && "Stack overflow in pushMove");
            return;
        }
        if (move.from >= 64 || move.to >= 64) {
            std::cerr << "ERROR: pushMove invalid move! from=" << move.from << " to=" << move.to << std::endl;
            assert(false && "Invalid move in pushMove");
            return;
        }
        srcSquares[stackLevel] = move.from;
        dstSquares[stackLevel] = move.to;
        saveMoves[stackLevel] = state[dstSquares[stackLevel]];
        state[dstSquares[stackLevel]] = state[srcSquares[stackLevel]];
        state[srcSquares[stackLevel]] = '0';
        ++stackLevel;
    }
    void popMove() {
        if (stackLevel <= 0) {
            std::cerr << "ERROR: popMove stack underflow! stackLevel=" << stackLevel << std::endl;
            assert(false && "Stack underflow in popMove");
            return;
        }
        --stackLevel;
        state[srcSquares[stackLevel]] = state[dstSquares[stackLevel]];
        state[dstSquares[stackLevel]] = saveMoves[stackLevel];
    }
};

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;
    void bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;
    bool  gameHasAI() override { return _gameHasAI; }
    // Enable or disable AI for this Chess instance (AI is always Black when enabled).
    void setUseAI(bool useAI) { _gameHasAI = useAI; }
    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;

    std::vector<BitMove> generateAllMoves();
    void generateKnightMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t friendlyPieces);
    void generateKnightMoves(std::vector<BitMove>& moves, std::string& state);
    void generatePawnMoves(std::vector<BitMove>& moves, uint64_t pawnBoard, int player);
    void generateKingMoves(std::vector<BitMove>& moves, uint64_t kingBoard, uint64_t friendlyPieces, uint64_t dangerSquares);
    uint64_t computeAttackedSquares(int byPlayer) const;
    void generateBishopMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies);
    void generateQueenMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies);
    void generateRookMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies);

    // AI helpers (negamax on GameState; implementation in Chess.cpp)
    int Evaluate(GameState& state);
    int negamax(GameState& state, int depth, int alpha, int beta, int playerColor);
    std::vector<BitMove> generateAllMovesFromState(GameState& state, int playerColor);
    void updateAI() override;

    Grid* _grid;
    std::vector<BitMove> _moves;         
    uint64_t _bitboards[e_numBitBoards];
    int _bitboardLookup[128];           

    // this returns the the square index where capture is legal for enpassant or -1 if not.
    int _enPassantSquare;

    // these checks are for castling, if any of these are set to true then we can't castle
    bool _whiteKingHasMoved;
    bool _blackKingHasMoved;
    bool _whiteKingsideRookHasMoved;
    bool _whiteQueensideRookHasMoved;
    bool _blackKingsideRookHasMoved;
    bool _blackQueensideRookHasMoved;
    // Whether this Chess instance should use an AI opponent (Black).
    bool _gameHasAI;
};
