#pragma once


#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"

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
};
