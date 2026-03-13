#include "Chess.h"
#include "MagicBitboards.h"
#include "Evaluate.h"
#include <limits>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <cstring>


Chess::Chess()
{
    _grid = new Grid(8, 8);

    // this gamemode has optional AI
    _gameHasAI = true;

    // this initializes en passant as -1 meaning there are no valid en passant captures within 0-63 on board
    _enPassantSquare = -1;

    // these checks are for castling, if any of these are set to true then we can't castle
    _whiteKingHasMoved = false;
    _blackKingHasMoved = false;
    _whiteKingsideRookHasMoved = false;
    _whiteQueensideRookHasMoved = false;
    _blackKingsideRookHasMoved = false;
    _blackQueensideRookHasMoved = false;

    memset(_bitboardLookup, 0, sizeof(_bitboardLookup));
    // Map FEN chars to bitboard indices for easy setup and state parsing
    _bitboardLookup['P'] = WHITE_PAWNS;
    _bitboardLookup['N'] = WHITE_KNIGHTS;
    _bitboardLookup['B'] = WHITE_BISHOPS;
    _bitboardLookup['R'] = WHITE_ROOKS;
    _bitboardLookup['Q'] = WHITE_QUEENS;
    _bitboardLookup['K'] = WHITE_KINGS;
    _bitboardLookup['p'] = BLACK_PAWNS;
    _bitboardLookup['n'] = BLACK_KNIGHTS;
    _bitboardLookup['b'] = BLACK_BISHOPS;
    _bitboardLookup['r'] = BLACK_ROOKS;
    _bitboardLookup['q'] = BLACK_QUEENS;
    _bitboardLookup['k'] = BLACK_KINGS;
}

Chess::~Chess()
{
    delete _grid;
}


char Chess::pieceNotation(int x, int y) const
{
    Bit *bit = _grid->getSquare(x, y)->bit();
    if (!bit) {
        return '0';
    }

    // gameTag encodes color in bit 7 and piece type in the low bits
    int tag = bit->gameTag();
    int pieceType = tag & 127;      
    bool isBlack  = (tag & 128) != 0; 

    char c = '0';
    switch (pieceType) {
        case Pawn:   c = 'P'; break;
        case Knight: c = 'N'; break;
        case Bishop: c = 'B'; break;
        case Rook:   c = 'R'; break;
        case Queen:  c = 'Q'; break;
        case King:   c = 'K'; break;
        default:
            // Unknown piece type
            return '0';
    }

    if (isBlack) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return c;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    //Reset all the speacial move states so that it doesn't affect the next game
    _enPassantSquare = -1;
    _whiteKingHasMoved = false;
    _blackKingHasMoved = false;
    _whiteKingsideRookHasMoved = false;
    _whiteQueensideRookHasMoved = false;
    _blackKingsideRookHasMoved = false;
    _blackQueensideRookHasMoved = false;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

   // only black can be played by the AI
    if (gameHasAI()) {
        setAIPlayer(AI_PLAYER);
    }

    startGame();
    _moves = generateAllMoves();
}

// we go through FEN and translate it to the board setup
void Chess::FENtoBoard(const std::string& fen) {

    std::string boardStr = fen.substr(0, fen.find(' ')); 
    // FEN reads from top to bottom and left to right so X is left and y is rank 8
    int x = 0;
    int y = 7;
    for (char c : boardStr) {
        if (c == '/') {
            // slash moves to the next rank, we reset the x
            x = 0;
            y--;
        } else if (c >= '1' && c <= '8') {
            // digits represent empty squares, so we just move forward by that amount
            x += c - '0';
        } else {
            // last case is that its a piece, so we place it on the board
            int player = (c >= 'a' && c <= 'z') ? 1 : 0;
            ChessPiece piece = NoPiece;
            // map the FEN characters to our enum types
            switch (c) {
                case 'p': case 'P': piece = Pawn; break;
                case 'n': case 'N': piece = Knight; break;
                case 'b': case 'B': piece = Bishop; break;
                case 'r': case 'R': piece = Rook; break;
                case 'q': case 'Q': piece = Queen; break;
                case 'k': case 'K': piece = King; break;
            }
            if (piece != NoPiece) {
                Bit* bit = PieceForPlayer(player, piece);
                bit->setGameTag(piece + (player * 128));
                ChessSquare* square = _grid->getSquare(x, y);
                square->setBit(bit);
                bit->moveTo(square->getPosition());
            }
            x++;
        }
    }
}


bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    //
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;

    // bitwise and gives us the piece color
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor != currentPlayer) return false;

    // bitwise and with 127 gives us the piece type
    int pieceType = bit.gameTag() & 127;
    // check if piece ttuyp is valid
    if (pieceType != Knight &&
        pieceType != Pawn &&
        pieceType != King &&
        pieceType != Bishop &&
        pieceType != Rook &&
        pieceType != Queen) {
        return true;
    }

    // check the precomputed moves for the piece to see if it can move from this square
    ChessSquare* square = (ChessSquare *)&src;
    int squareIndex = square->getSquareIndex();
    for (const auto& move : _moves) {
        if (move.from == squareIndex) {
            return true;
        }
    }
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    int pieceType = bit.gameTag() & 127;
    if (pieceType != Knight &&
        pieceType != Pawn &&
        pieceType != King &&
        pieceType != Bishop &&
        pieceType != Rook &&
        pieceType != Queen) {
        return true;
    }

    ChessSquare* squareSrc = (ChessSquare *)&src;
    ChessSquare* squareDst = (ChessSquare *)&dst;
    int fromIndex = squareSrc->getSquareIndex();
    int toIndex = squareDst->getSquareIndex();

    // search through move list to see if this move is legal
    for (const auto& move : _moves) {
        if (move.from == fromIndex && move.to == toIndex) {
            return true;
        }
    }
    return false;
}


void Chess::bitMovedFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    int pieceType = bit.gameTag() & 127;
    int color = (bit.gameTag() & 128) ? 1 : 0; 

    // we need to save this because the new one will overwrite it
    int oldEnPassant = _enPassantSquare;

    ChessSquare* dstSquare = (ChessSquare*)&dst;
    int toIndex = dstSquare->getSquareIndex();
    ChessSquare* srcSquare = (ChessSquare*)&src;
    int fromIndex = srcSquare->getSquareIndex();

    // keep track of castling rights, if a king or rook moves for a color it is lost permanently
    if (pieceType == King) {
        if (color == 0) {
            _whiteKingHasMoved = true;
        } else {
            _blackKingHasMoved = true;
        }
    } else if (pieceType == Rook) {
        if (color == 0) {
            if (fromIndex == 0) _whiteQueensideRookHasMoved = true;
            if (fromIndex == 7) _whiteKingsideRookHasMoved = true;
        } else {
            if (fromIndex == 56) _blackQueensideRookHasMoved = true;
            if (fromIndex == 63) _blackKingsideRookHasMoved = true;
        }
    }

    // execute the rook move for castling if the king has moved two spaces
    if (pieceType == King) {
        int fromFile = fromIndex % 8;
        int toFile = toIndex % 8;
        if (std::abs(toFile - fromFile) == 2) {
            int rookFrom = -1;
            int rookTo = -1;

            // determine which rook to move using the color and the direction of the king
            if (color == 0) {
                // white
                if (toFile == 6) {            
                    rookFrom = 7;             
                    rookTo   = 5;             
                    _whiteKingsideRookHasMoved = true;
                } else if (toFile == 2) {     
                    rookFrom = 0;            
                    rookTo   = 3;             
                    _whiteQueensideRookHasMoved = true;
                }
            } else {
                // black
                if (toFile == 6) {            
                    rookFrom = 63;            
                    rookTo   = 61;            
                    _blackKingsideRookHasMoved = true;
                } else if (toFile == 2) {     
                    rookFrom = 56;            
                    rookTo   = 59;           
                    _blackQueensideRookHasMoved = true;
                }
            }

            // move the rook if needed
            if (rookFrom >= 0 && rookTo >= 0) {
                int rfx = rookFrom % 8;
                int rfy = rookFrom / 8;
                int rtx = rookTo % 8;
                int rty = rookTo / 8;
                ChessSquare* rookFromSquare = _grid->getSquare(rfx, rfy);
                ChessSquare* rookToSquare = _grid->getSquare(rtx, rty);
                if (rookFromSquare && rookToSquare) {
                    Bit* rookBit = rookFromSquare->bit();
                    // Only move if there is actually a rook of the same color here.
                    if (rookBit) {
                        int rookTag = rookBit->gameTag();
                        int rookColor = (rookTag & 128) ? 1 : 0;
                        int rookType  = rookTag & 127;
                        if (rookColor == color && rookType == Rook) {
                            rookFromSquare->releaseBit();
                            rookToSquare->setBit(rookBit);
                            rookBit->moveTo(rookToSquare->getPosition());
                        }
                    }
                }
            }
        }
    }

    // if a pawn moves to the en passant square, we need to remove the captured pawn
    if (pieceType == Pawn && oldEnPassant >= 0 && toIndex == oldEnPassant) {
        int capturedIndex = (color == 0) ? (toIndex - 8) : (toIndex + 8);
        int cx = capturedIndex % 8;
        int cy = capturedIndex / 8;
        ChessSquare* capturedSquare = _grid->getSquare(cx, cy);
        if (capturedSquare && capturedSquare->bit()) {
            capturedSquare->destroyBit();
        }
    }

    // reset it, because en passant only lasts for one turn
    _enPassantSquare = -1;
    if (pieceType == Pawn) {
        int fromRank = fromIndex / 8;
        int toRank = toIndex / 8;

        // if the pawn moves two forward, we set the en passant square 
        if (std::abs(toRank - fromRank) == 2) {
            int midRank = (fromRank + toRank) / 2;
            int file = fromIndex % 8;
            _enPassantSquare = midRank * 8 + file;
        }
    }

    // if a pawn reachest the last rank we promote it to queen
    if (pieceType == Pawn) {
        int rank = toIndex / 8;
        bool promote = (color == 0 && rank == 7) || (color == 1 && rank == 0);
        if (promote) {
            dstSquare->destroyBit();
            Bit* newBit = PieceForPlayer(color, Queen);
            newBit->setGameTag(Queen + (color * 128));
            dstSquare->setBit(newBit);
            newBit->moveTo(dstSquare->getPosition());
        }
    }

    // Debug: print board state (FEN-like) after every move
    // std::string boardState = stateString();
    // std::cout << "\n--- Board after move ---\n";
    // for (int rank = 7; rank >= 0; --rank) {
    //     std::cout << (rank + 1) << " ";
    //     for (int file = 0; file < 8; ++file) {
    //         char c = boardState[rank * 8 + file];
    //         std::cout << (c == '0' ? '.' : c) << " ";
    //     }
    //     std::cout << "\n";
    // }
    // std::cout << "  a b c d e f g h\n";
    // std::cout << "State: " << boardState << "\n";


    Game::bitMovedFromTo(bit, src, dst);
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    // make sure we are in the board.
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{

    _moves = generateAllMoves();

    if (_moves.empty()) {
        int currentPlayer = getCurrentPlayer()->playerNumber(); // 0=white, 1=black
        int kingIndex = (currentPlayer == 0) ? WHITE_KINGS : BLACK_KINGS;
        uint64_t kingBoard = _bitboards[kingIndex];

        int opponent = 1 - currentPlayer;
        uint64_t dangerSquares = computeAttackedSquares(opponent);

        // King is in check and no legal moves means checkmate
        if (kingBoard & dangerSquares) {
            return getPlayerAt(opponent);
        }
    }
    return nullptr;
}

bool Chess::checkForDraw()
{

    if (_moves.empty()) {
        int currentPlayer = getCurrentPlayer()->playerNumber();
        int kingIndex = (currentPlayer == 0) ? WHITE_KINGS : BLACK_KINGS;
        uint64_t kingBoard = _bitboards[kingIndex];

        int opponent = 1 - currentPlayer;
        uint64_t dangerSquares = computeAttackedSquares(opponent);

        if (!(kingBoard & dangerSquares)) {
            return true;
        }
    }
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);

    // iterate thorugh the baord and append the FEN characters
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char c = s[index];
        if (c == '0') {
            square->destroyBit();
            return;
        }

        // lowercase for black and uppercase for white
        int player = std::islower(c) ? 1 : 0;

        ChessPiece piece = NoPiece;
        switch (std::tolower(c)) {
            case 'p': piece = Pawn; break;
            case 'n': piece = Knight; break;
            case 'b': piece = Bishop; break;
            case 'r': piece = Rook; break;
            case 'q': piece = Queen; break;
            case 'k': piece = King; break;
            default:  piece = NoPiece; break;
        }

        //clear the square first
        square->destroyBit();
        // if the char is not 0 (board not empty), then we add the piece to the board
        if (piece != NoPiece) {
            Bit* bit = PieceForPlayer(player, piece);
            bit->setGameTag(piece + (player * 128));
            square->setBit(bit);
            bit->moveTo(square->getPosition());
        }
    });
}


void Chess::generateKnightMoves(std::vector<BitMove>& moves, BitBoard knightBoard, uint64_t friendlyPieces){
    knightBoard.forEachBit([&](int fromSquare) {
        uint64_t attacks = KnightAttacks[fromSquare] & ~friendlyPieces;
        BitBoard moveBitboard(attacks);
        moveBitboard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
}


void Chess::generatePawnMoves(std::vector<BitMove>& moves, uint64_t pawnBoard, int player) {
    uint64_t emptySquares = ~_bitboards[OCCUPANCY];
    uint64_t enemyPieces = _bitboards[(player == 0) ? BLACK_ALL_PIECES : WHITE_ALL_PIECES];

    if (player == 0) {
        uint64_t singlePush = NORTH(pawnBoard) & emptySquares;
        BitBoard(singlePush).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 8, toSquare, Pawn);
        });

        uint64_t doublePush = NORTH(singlePush) & emptySquares & 0x00000000FF000000ULL;
        BitBoard(doublePush).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 16, toSquare, Pawn);
        });

        uint64_t captureNE = NORTH_EAST(pawnBoard) & enemyPieces;
        BitBoard(captureNE).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 9, toSquare, Pawn);
        });

        uint64_t captureNW = NORTH_WEST(pawnBoard) & enemyPieces;
        BitBoard(captureNW).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 7, toSquare, Pawn);
        });

       
        if (_enPassantSquare >= 0) {
            int ep = _enPassantSquare;
            int epFile = ep % 8;
            int epRank = ep / 8;
            
            if (epFile > 0 && epRank > 0) {
                int fromSquare = (epRank - 1) * 8 + (epFile - 1);
                if (pawnBoard & (1ULL << fromSquare)) {
                    moves.emplace_back(fromSquare, ep, Pawn);
                }
            }
           
            if (epFile < 7 && epRank > 0) {
                int fromSquare = (epRank - 1) * 8 + (epFile + 1);
                if (pawnBoard & (1ULL << fromSquare)) {
                    moves.emplace_back(fromSquare, ep, Pawn);
                }
            }
        }
    } else {
        uint64_t singlePush = SOUTH(pawnBoard) & emptySquares;
        BitBoard(singlePush).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 8, toSquare, Pawn);
        });

        uint64_t doublePush = SOUTH(singlePush) & emptySquares & 0x000000FF00000000ULL;
        BitBoard(doublePush).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 16, toSquare, Pawn);
        });

        uint64_t captureSE = SOUTH_EAST(pawnBoard) & enemyPieces;
        BitBoard(captureSE).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 7, toSquare, Pawn);
        });

        uint64_t captureSW = SOUTH_WEST(pawnBoard) & enemyPieces;
        BitBoard(captureSW).forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 9, toSquare, Pawn);
        });

       
        if (_enPassantSquare >= 0) {
            int ep = _enPassantSquare;
            int epFile = ep % 8;
            int epRank = ep / 8;
            if (epFile > 0 && epRank < 7) {
                int fromSquare = (epRank + 1) * 8 + (epFile - 1);
                if (pawnBoard & (1ULL << fromSquare)) {
                    moves.emplace_back(fromSquare, ep, Pawn);
                }
            }
            if (epFile < 7 && epRank < 7) {
                int fromSquare = (epRank + 1) * 8 + (epFile + 1);
                if (pawnBoard & (1ULL << fromSquare)) {
                    moves.emplace_back(fromSquare, ep, Pawn);
                }
            }
        }
    }
}

uint64_t Chess::computeAttackedSquares(int byPlayer) const {
    uint64_t attacked = 0;
    uint64_t occupancy = _bitboards[OCCUPANCY];

    // pawn attacks
    if (byPlayer == 0) {
        attacked |= WHITE_PAWN_ATTACKS(_bitboards[WHITE_PAWNS]);
    } else {
        attacked |= BLACK_PAWN_ATTACKS(_bitboards[BLACK_PAWNS]);
    }

    // knight attacks
    int knightIdx = (byPlayer == 0) ? WHITE_KNIGHTS : BLACK_KNIGHTS;
    BitBoard(_bitboards[knightIdx]).forEachBit([&](int sq) {
        attacked |= KnightAttacks[sq];
    });

    // bishop attacks
    int bishopIdx = (byPlayer == 0) ? WHITE_BISHOPS : BLACK_BISHOPS;
    BitBoard(_bitboards[bishopIdx]).forEachBit([&](int sq) {
        attacked |= getBishopAttacks(sq, occupancy);
    });

    // rook attacks
    int rookIdx = (byPlayer == 0) ? WHITE_ROOKS : BLACK_ROOKS;
    BitBoard(_bitboards[rookIdx]).forEachBit([&](int sq) {
        attacked |= getRookAttacks(sq, occupancy);
    });

    // queen attacks
    int queenIdx = (byPlayer == 0) ? WHITE_QUEENS : BLACK_QUEENS;
    BitBoard(_bitboards[queenIdx]).forEachBit([&](int sq) {
        attacked |= getQueenAttacks(sq, occupancy);
    });

    // king attacks
    int kingIdx = (byPlayer == 0) ? WHITE_KINGS : BLACK_KINGS;
    BitBoard(_bitboards[kingIdx]).forEachBit([&](int sq) {
        attacked |= KingAttacks[sq];
    });

    return attacked;
}

void Chess::generateKingMoves(std::vector<BitMove>& moves, uint64_t kingBoard, uint64_t friendlyPieces, uint64_t dangerSquares) {
    int player = getCurrentPlayer()->playerNumber();
    BitBoard(kingBoard).forEachBit([&](int fromSquare) {
        // 8-way king moves but we remove any occupied by friendly pieces and any that are attacked
        uint64_t attacks = KingAttacks[fromSquare] & ~friendlyPieces & ~dangerSquares;
        BitBoard(attacks).forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, King);
        });

        // Castling
        bool kingHasMoved = (player == 0) ? _whiteKingHasMoved : _blackKingHasMoved;
        if (kingHasMoved) return;

        uint64_t occ = _bitboards[OCCUPANCY];

        if (player == 0 && fromSquare == 4) { // white king on e1
            // Print once per turn only when the path is clear but castling is still blocked.
            static unsigned int lastCastleDebugTurnWhite = 0xFFFFFFFFu;


            const bool rookPresentK = (_bitboards[WHITE_ROOKS] & (1ULL << 7)) != 0;
            const bool pathEmptyK = (occ & ((1ULL << 5) | (1ULL << 6))) == 0;
            const bool safeSquaresK = (dangerSquares & ((1ULL << 4) | (1ULL << 5) | (1ULL << 6))) == 0;
            const bool rightsK = !_whiteKingsideRookHasMoved && !_whiteKingHasMoved;
            const bool canCastleK = rightsK && rookPresentK && pathEmptyK && safeSquaresK;


            const bool rookPresentQ = (_bitboards[WHITE_ROOKS] & (1ULL << 0)) != 0;
            const bool pathEmptyQ = (occ & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3))) == 0;
            const bool safeSquaresQ = (dangerSquares & ((1ULL << 4) | (1ULL << 3) | (1ULL << 2))) == 0;
            const bool rightsQ = !_whiteQueensideRookHasMoved && !_whiteKingHasMoved;
            const bool canCastleQ = rightsQ && rookPresentQ && pathEmptyQ && safeSquaresQ;

            // Always print once per turn whenever the white king is on e1,
            // so we can see full castling state even when the path is not yet clear.
            if (lastCastleDebugTurnWhite != _gameOptions.currentTurnNo) {
                lastCastleDebugTurnWhite = _gameOptions.currentTurnNo;

                // If castling is blocked due to attacked squares, try to identify a concrete attacker.
                auto findAttackerForTarget = [&](int opponent, int targetSq) -> std::string {
                    const uint64_t targetBit = (1ULL << targetSq);

                    // Pawns: compute likely source squares for the target.
                    uint64_t oppPawns = _bitboards[(opponent == 0) ? WHITE_PAWNS : BLACK_PAWNS];
                    if (opponent == 0) {
                        // White pawns attack (sq+7, sq+9). If target is attacked, a pawn must be at target-7 or target-9.
                        int s1 = targetSq - 7;
                        int s2 = targetSq - 9;
                        if (s1 >= 0 && s1 < 64 && (oppPawns & (1ULL << s1))) return "pawn@" + std::to_string(s1);
                        if (s2 >= 0 && s2 < 64 && (oppPawns & (1ULL << s2))) return "pawn@" + std::to_string(s2);
                    } else {
                        // Black pawns attack (sq-7, sq-9). If target is attacked, a pawn must be at target+7 or target+9.
                        int s1 = targetSq + 7;
                        int s2 = targetSq + 9;
                        if (s1 >= 0 && s1 < 64 && (oppPawns & (1ULL << s1))) return "pawn@" + std::to_string(s1);
                        if (s2 >= 0 && s2 < 64 && (oppPawns & (1ULL << s2))) return "pawn@" + std::to_string(s2);
                    }

                    // Knights
                    uint64_t oppKnights = _bitboards[(opponent == 0) ? WHITE_KNIGHTS : BLACK_KNIGHTS];
                    int knightSq = -1;
                    BitBoard(oppKnights).forEachBit([&](int sq) {
                        if (knightSq < 0 && (KnightAttacks[sq] & targetBit)) knightSq = sq;
                    });
                    if (knightSq >= 0) return "knight@" + std::to_string(knightSq);

                    // Bishops
                    uint64_t occAll = _bitboards[OCCUPANCY];
                    uint64_t oppBishops = _bitboards[(opponent == 0) ? WHITE_BISHOPS : BLACK_BISHOPS];
                    int bishopSq = -1;
                    BitBoard(oppBishops).forEachBit([&](int sq) {
                        if (bishopSq < 0 && (getBishopAttacks(sq, occAll) & targetBit)) bishopSq = sq;
                    });
                    if (bishopSq >= 0) return "bishop@" + std::to_string(bishopSq);

                    // Rooks
                    uint64_t oppRooks = _bitboards[(opponent == 0) ? WHITE_ROOKS : BLACK_ROOKS];
                    int rookSq = -1;
                    BitBoard(oppRooks).forEachBit([&](int sq) {
                        if (rookSq < 0 && (getRookAttacks(sq, occAll) & targetBit)) rookSq = sq;
                    });
                    if (rookSq >= 0) return "rook@" + std::to_string(rookSq);

                    // Queens
                    uint64_t oppQueens = _bitboards[(opponent == 0) ? WHITE_QUEENS : BLACK_QUEENS];
                    int queenSq = -1;
                    BitBoard(oppQueens).forEachBit([&](int sq) {
                        if (queenSq < 0 && (getQueenAttacks(sq, occAll) & targetBit)) queenSq = sq;
                    });
                    if (queenSq >= 0) return "queen@" + std::to_string(queenSq);

                    // King (shouldn't normally matter for castling unless adjacent)
                    uint64_t oppKing = _bitboards[(opponent == 0) ? WHITE_KINGS : BLACK_KINGS];
                    int kingSq = -1;
                    BitBoard(oppKing).forEachBit([&](int sq) {
                        if (kingSq < 0 && (KingAttacks[sq] & targetBit)) kingSq = sq;
                    });
                    if (kingSq >= 0) return "king@" + std::to_string(kingSq);

                    return "unknown";
                };

                const int opponent = 1; // white to move => opponent is black in UI movegen context
                std::string atkE1 = ((dangerSquares & (1ULL << 4)) ? findAttackerForTarget(opponent, 4) : "none");
                std::string atkF1 = ((dangerSquares & (1ULL << 5)) ? findAttackerForTarget(opponent, 5) : "none");
                std::string atkG1 = ((dangerSquares & (1ULL << 6)) ? findAttackerForTarget(opponent, 6) : "none");

                std::cout << "[CASTLE BLOCK] White: "
                          << "kingMoved=" << (_whiteKingHasMoved ? 1 : 0)
                          << " rookKMoved=" << (_whiteKingsideRookHasMoved ? 1 : 0)
                          << " rookQMoved=" << (_whiteQueensideRookHasMoved ? 1 : 0)
                          << " rookKPresent=" << (rookPresentK ? 1 : 0)
                          << " rookQPresent=" << (rookPresentQ ? 1 : 0)
                          << " pathK=" << (pathEmptyK ? 1 : 0)
                          << " pathQ=" << (pathEmptyQ ? 1 : 0)
                          << " safeK=" << (safeSquaresK ? 1 : 0)
                          << " safeQ=" << (safeSquaresQ ? 1 : 0)
                          << " atkE1=" << atkE1
                          << " atkF1=" << atkF1
                          << " atkG1=" << atkG1
                          << " danger(e1f1g1)=" << ((dangerSquares >> 4) & 0x7)
                          << " danger(c1d1e1)=" << ((dangerSquares >> 2) & 0x7)
                          << " occ(e1f1g1)=" << ((occ >> 4) & 0x7)
                          << " occ(b1c1d1)=" << ((occ >> 1) & 0x7)
                          << "\n";
            }

            // Kingside castle: e1 -> g1, rook h1 -> f1
            if (!_whiteKingsideRookHasMoved &&
                (_bitboards[WHITE_ROOKS] & (1ULL << 7)) &&
                !(occ & ((1ULL << 5) | (1ULL << 6))) &&
                !(dangerSquares & ((1ULL << 4) | (1ULL << 5) | (1ULL << 6)))) {
                moves.emplace_back(4, 6, King);
            }
            // Queenside castle: e1 -> c1, rook a1 -> d1
            if (!_whiteQueensideRookHasMoved &&
                (_bitboards[WHITE_ROOKS] & (1ULL << 0)) &&
                !(occ & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3))) &&
                !(dangerSquares & ((1ULL << 4) | (1ULL << 3) | (1ULL << 2)))) {
                moves.emplace_back(4, 2, King);
            }
        } else if (player == 1 && fromSquare == 60) { // black king on e8
            static unsigned int lastCastleDebugTurnBlack = 0xFFFFFFFFu;

            const bool rookPresent = (_bitboards[BLACK_ROOKS] & (1ULL << 63)) != 0;
            const bool pathEmpty = (occ & ((1ULL << 61) | (1ULL << 62))) == 0;
            const bool safeSquares = (dangerSquares & ((1ULL << 60) | (1ULL << 61) | (1ULL << 62))) == 0;
            const bool rights = !_blackKingsideRookHasMoved && !_blackKingHasMoved;

            const bool canCastleK = rights && rookPresent && pathEmpty && safeSquares;
            if (!canCastleK && lastCastleDebugTurnBlack != _gameOptions.currentTurnNo) {
                lastCastleDebugTurnBlack = _gameOptions.currentTurnNo;
                std::cout << "[CASTLE BLOCK] Black K-side: "
                          << "kingMoved=" << (_blackKingHasMoved ? 1 : 0)
                          << " rookMoved=" << (_blackKingsideRookHasMoved ? 1 : 0)
                          << " rookPresent=" << (rookPresent ? 1 : 0)
                          << " pathEmpty=" << (pathEmpty ? 1 : 0)
                          << " safe(e8f8g8)=" << (safeSquares ? 1 : 0)
                          << " dangerMask=" << ((dangerSquares >> 60) & 0x7)
                          << " occMask=" << ((occ >> 60) & 0x7)
                          << "\n";
            }

            // Kingside castle: e8 -> g8, rook h8 -> f8
            if (!_blackKingsideRookHasMoved &&
                (_bitboards[BLACK_ROOKS] & (1ULL << 63)) &&
                !(occ & ((1ULL << 61) | (1ULL << 62))) &&
                !(dangerSquares & ((1ULL << 60) | (1ULL << 61) | (1ULL << 62)))) {
                moves.emplace_back(60, 62, King);
            }
            // Queenside castle: e8 -> c8, rook a8 -> d8
            if (!_blackQueensideRookHasMoved &&
                (_bitboards[BLACK_ROOKS] & (1ULL << 56)) &&
                !(occ & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59))) &&
                !(dangerSquares & ((1ULL << 60) | (1ULL << 59) | (1ULL << 58)))) {
                moves.emplace_back(60, 58, King);
            }
        }
    });
}

void Chess::generateBishopMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies) {
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitBoard = BitBoard(getBishopAttacks(fromSquare, occupancy) & ~friendlies);
        moveBitBoard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Bishop);
        });
    });
}

void Chess::generateRookMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies) {
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitBoard = BitBoard(getRookAttacks(fromSquare, occupancy) & ~friendlies);
        moveBitBoard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Rook);
        });
    });
}

void Chess::generateQueenMoves(std::vector<BitMove>& moves, BitBoard piecesBoard, uint64_t occupancy, uint64_t friendlies) {
    piecesBoard.forEachBit([&](int fromSquare) {
        BitBoard moveBitBoard = BitBoard(getQueenAttacks(fromSquare, occupancy) & ~friendlies);
        moveBitBoard.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Queen);
        });
    });
}


std::vector<BitMove> Chess::generateAllMoves(){
    static bool magicInit = false;
    if (!magicInit) {
        initMagicBitboards(); 
        magicInit = true;
    }

    std::vector<BitMove> moves;
    moves.reserve(32);
    std::string state = stateString();

    for (int i = 0; i < e_numBitBoards; i++) {
        _bitboards[i] = 0;
    }

    for (int i = 0; i < 64; i++) {
        if (state[i] != '0') {
            unsigned char c = static_cast<unsigned char>(state[i]);
            int bitIndex = _bitboardLookup[c];
            if (bitIndex < 0 || bitIndex >= e_numBitBoards) {
                std::cerr << "ERROR: generateAllMoves: invalid bitIndex="
                          << bitIndex << " for char '" << state[i]
                          << "' at square " << i << std::endl;
                assert(false && "Invalid bitIndex in _bitboardLookup (UI path)");
                continue;
            }
            _bitboards[bitIndex] |= (1ULL << i);
            _bitboards[OCCUPANCY] |= (1ULL << i);
            _bitboards[std::isupper(state[i]) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES] |= (1ULL << i);
        }
    }

    int player = getCurrentPlayer()->playerNumber();
    int knightIndex = (player == 0) ? WHITE_KNIGHTS : BLACK_KNIGHTS;
    int friendlyAll = (player == 0) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES;

    generateKnightMoves(moves, BitBoard(_bitboards[knightIndex]), _bitboards[friendlyAll]);

    int bishopIndex = (player == 0) ? WHITE_BISHOPS : BLACK_BISHOPS;
    generateBishopMoves(moves, BitBoard(_bitboards[bishopIndex]), _bitboards[OCCUPANCY], _bitboards[friendlyAll]);

    int rookIndex = (player == 0) ? WHITE_ROOKS : BLACK_ROOKS;
    generateRookMoves(moves, BitBoard(_bitboards[rookIndex]), _bitboards[OCCUPANCY], _bitboards[friendlyAll]);

    int queenIndex = (player == 0) ? WHITE_QUEENS : BLACK_QUEENS;
    generateQueenMoves(moves, BitBoard(_bitboards[queenIndex]), _bitboards[OCCUPANCY], _bitboards[friendlyAll]);

    int pawnIndex = (player == 0) ? WHITE_PAWNS : BLACK_PAWNS;
    generatePawnMoves(moves, _bitboards[pawnIndex], player);

    int opponent = 1 - player;
    uint64_t dangerSquares = computeAttackedSquares(opponent);

    int kingIndex = (player == 0) ? WHITE_KINGS : BLACK_KINGS;
    generateKingMoves(moves, _bitboards[kingIndex], _bitboards[friendlyAll], dangerSquares);

    uint64_t ownKing = _bitboards[kingIndex];
    int enemyAll = (player == 0) ? BLACK_ALL_PIECES : WHITE_ALL_PIECES;
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const BitMove& move) {
        uint64_t newOccupancy  = (_bitboards[OCCUPANCY]   & ~(1ULL << move.from)) | (1ULL << move.to);
        uint64_t newFriendly   = (_bitboards[friendlyAll] & ~(1ULL << move.from)) | (1ULL << move.to);
        uint64_t newEnemy      =  _bitboards[enemyAll]    & ~(1ULL << move.to);

        uint64_t kingAfter = (move.piece == King)
            ? (1ULL << move.to)
            : ownKing;

        // pawns
        uint64_t oppPawns = _bitboards[(opponent == 0) ? WHITE_PAWNS : BLACK_PAWNS] & ~(1ULL << move.to);
        uint64_t pawnThreat = (opponent == 0)
            ? WHITE_PAWN_ATTACKS(oppPawns)
            : BLACK_PAWN_ATTACKS(oppPawns);
        if (kingAfter & pawnThreat) return true;

        // Knights
        uint64_t oppKnights = _bitboards[(opponent == 0) ? WHITE_KNIGHTS : BLACK_KNIGHTS] & ~(1ULL << move.to);
        uint64_t knightThreat = 0;
        BitBoard(oppKnights).forEachBit([&](int sq){ knightThreat |= KnightAttacks[sq]; });
        if (kingAfter & knightThreat) return true;

        // Bishops
        uint64_t oppBishops = _bitboards[(opponent == 0) ? WHITE_BISHOPS : BLACK_BISHOPS] & ~(1ULL << move.to);
        uint64_t bishopThreat = 0;
        BitBoard(oppBishops).forEachBit([&](int sq){ bishopThreat |= getBishopAttacks(sq, newOccupancy); });
        if (kingAfter & bishopThreat) return true;

        // Rooks
        uint64_t oppRooks = _bitboards[(opponent == 0) ? WHITE_ROOKS : BLACK_ROOKS] & ~(1ULL << move.to);
        uint64_t rookThreat = 0;
        BitBoard(oppRooks).forEachBit([&](int sq){ rookThreat |= getRookAttacks(sq, newOccupancy); });
        if (kingAfter & rookThreat) return true;

        // queen
        uint64_t oppQueens = _bitboards[(opponent == 0) ? WHITE_QUEENS : BLACK_QUEENS] & ~(1ULL << move.to);
        uint64_t queenThreat = 0;
        BitBoard(oppQueens).forEachBit([&](int sq){ queenThreat |= getQueenAttacks(sq, newOccupancy); });
        if (kingAfter & queenThreat) return true;

        // Enemy king to prevent illegal king moves
        uint64_t oppKing = _bitboards[(opponent == 0) ? WHITE_KINGS : BLACK_KINGS];
        uint64_t kingThreat = 0;
        BitBoard(oppKing).forEachBit([&](int sq){ kingThreat |= KingAttacks[sq]; });
        if (kingAfter & kingThreat) return true;

        return false;
    }), moves.end());

    return moves;
}

// --------------------------------------------------------------------------------
// AI helpers: Evaluate, generateAllMovesFromState, negamax, updateAI
// These operate purely on GameState (char[64] board) and do not touch Grid/Bit.
// --------------------------------------------------------------------------------

int Chess::Evaluate(GameState& state) {
    // Simple material evaluation using piece values.
    static int pieceValue[256];
    static bool init = false;
    if (!init) {
        init = true;
        for (int i = 0; i < 256; ++i) pieceValue[i] = 0;
        pieceValue['P'] = 100;  pieceValue['p'] = -100;
        pieceValue['N'] = 300;  pieceValue['n'] = -300;
        pieceValue['B'] = 325;  pieceValue['b'] = -325;
        pieceValue['R'] = 500;  pieceValue['r'] = -500;
        pieceValue['Q'] = 900;  pieceValue['q'] = -900;
        pieceValue['K'] = 20000; pieceValue['k'] = -20000;
    }

    int value = 0;
    for (int i = 0; i < 64; ++i) {
        unsigned char c = static_cast<unsigned char>(state.state[i]);
        value += pieceValue[c];
    }
    return value;
}

std::vector<BitMove> Chess::generateAllMovesFromState(GameState& state, int playerColor) {
    static bool magicInit = false;
    if (!magicInit) {
        initMagicBitboards();
        magicInit = true;
    }

    std::vector<BitMove> moves;
    moves.reserve(32);


    for (int i = 0; i < e_numBitBoards; ++i) {
        _bitboards[i] = 0;
    }

    for (int i = 0; i < 64; ++i) {
        char c = state.state[i];
        if (c != '0') {
            int bitIndex = _bitboardLookup[static_cast<unsigned char>(c)];
            if (bitIndex < 0 || bitIndex >= e_numBitBoards) {
                std::cerr << "ERROR: generateAllMovesFromState: invalid bitIndex="
                          << bitIndex << " for char '" << c
                          << "' at square " << i << std::endl;
                assert(false && "Invalid bitIndex in _bitboardLookup (AI path)");
                continue;
            }
            _bitboards[bitIndex] |= (1ULL << i);
            _bitboards[OCCUPANCY] |= (1ULL << i);
            _bitboards[std::isupper(c) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES] |= (1ULL << i);
        }
    }

    int player = playerColor == 1 ? 0 : 1; // +1 => white(0), -1 => black(1)
    int knightIndex = (player == 0) ? WHITE_KNIGHTS : BLACK_KNIGHTS;
    int friendlyAll = (player == 0) ? WHITE_ALL_PIECES : BLACK_ALL_PIECES;

    generateKnightMoves(moves, BitBoard(_bitboards[knightIndex]), _bitboards[friendlyAll]);

    int bishopIndex = (player == 0) ? WHITE_BISHOPS : BLACK_BISHOPS;
    generateBishopMoves(moves, BitBoard(_bitboards[bishopIndex]), _bitboards[OCCUPANCY], _bitboards[friendlyAll]);

    int rookIndex = (player == 0) ? WHITE_ROOKS : BLACK_ROOKS;
    generateRookMoves(moves, BitBoard(_bitboards[rookIndex]), _bitboards[OCCUPANCY], _bitboards[friendlyAll]);

    int queenIndex = (player == 0) ? WHITE_QUEENS : BLACK_QUEENS;
    generateQueenMoves(moves, BitBoard(_bitboards[queenIndex]), _bitboards[OCCUPANCY], _bitboards[friendlyAll]);

    int pawnIndex = (player == 0) ? WHITE_PAWNS : BLACK_PAWNS;
    generatePawnMoves(moves, _bitboards[pawnIndex], player);

    int opponent = 1 - player;
    uint64_t dangerSquares = computeAttackedSquares(opponent);

    int kingIndex = (player == 0) ? WHITE_KINGS : BLACK_KINGS;
    generateKingMoves(moves, _bitboards[kingIndex], _bitboards[friendlyAll], dangerSquares);

    // King safety filter as in generateAllMoves.
    uint64_t ownKing = _bitboards[kingIndex];
    int enemyAll = (player == 0) ? BLACK_ALL_PIECES : WHITE_ALL_PIECES;
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const BitMove& move) {
        uint64_t newOccupancy  = (_bitboards[OCCUPANCY]   & ~(1ULL << move.from)) | (1ULL << move.to);
        uint64_t newFriendly   = (_bitboards[friendlyAll] & ~(1ULL << move.from)) | (1ULL << move.to);
        uint64_t newEnemy      =  _bitboards[enemyAll]    & ~(1ULL << move.to);

        uint64_t kingAfter = (move.piece == King)
            ? (1ULL << move.to)
            : ownKing;

        uint64_t oppPawns = _bitboards[(opponent == 0) ? WHITE_PAWNS : BLACK_PAWNS] & ~(1ULL << move.to);
        uint64_t pawnThreat = (opponent == 0)
            ? WHITE_PAWN_ATTACKS(oppPawns)
            : BLACK_PAWN_ATTACKS(oppPawns);
        if (kingAfter & pawnThreat) return true;

        uint64_t oppKnights = _bitboards[(opponent == 0) ? WHITE_KNIGHTS : BLACK_KNIGHTS] & ~(1ULL << move.to);
        uint64_t knightThreat = 0;
        BitBoard(oppKnights).forEachBit([&](int sq){ knightThreat |= KnightAttacks[sq]; });
        if (kingAfter & knightThreat) return true;

        uint64_t oppBishops = _bitboards[(opponent == 0) ? WHITE_BISHOPS : BLACK_BISHOPS] & ~(1ULL << move.to);
        uint64_t bishopThreat = 0;
        BitBoard(oppBishops).forEachBit([&](int sq){ bishopThreat |= getBishopAttacks(sq, newOccupancy); });
        if (kingAfter & bishopThreat) return true;

        uint64_t oppRooks = _bitboards[(opponent == 0) ? WHITE_ROOKS : BLACK_ROOKS] & ~(1ULL << move.to);
        uint64_t rookThreat = 0;
        BitBoard(oppRooks).forEachBit([&](int sq){ rookThreat |= getRookAttacks(sq, newOccupancy); });
        if (kingAfter & rookThreat) return true;

        uint64_t oppQueens = _bitboards[(opponent == 0) ? WHITE_QUEENS : BLACK_QUEENS] & ~(1ULL << move.to);
        uint64_t queenThreat = 0;
        BitBoard(oppQueens).forEachBit([&](int sq){ queenThreat |= getQueenAttacks(sq, newOccupancy); });
        if (kingAfter & queenThreat) return true;

        uint64_t oppKing = _bitboards[(opponent == 0) ? WHITE_KINGS : BLACK_KINGS];
        uint64_t kingThreat = 0;
        BitBoard(oppKing).forEachBit([&](int sq){ kingThreat |= KingAttacks[sq]; });
        if (kingAfter & kingThreat) return true;

        return false;
    }), moves.end());

    // Final safety check: ensure all generated moves are in 0..63.
    for (const auto& m : moves) {
        if (m.from < 0 || m.from >= 64 || m.to < 0 || m.to >= 64) {
            std::cerr << "ERROR: generateAllMovesFromState produced invalid move: from="
                      << m.from << " to=" << m.to
                      << " piece=" << m.piece
                      << " playerColor=" << playerColor << std::endl;
            std::cerr << "Current GameState: ";
            for (int i = 0; i < 64; ++i) std::cerr << state.state[i];
            std::cerr << std::endl;
            assert(false && "generateAllMovesFromState produced out-of-range move");
            break;
        }
    }

    return moves;
}

int Chess::negamax(GameState& state, int depth, int alpha, int beta, int playerColor) {
    if (depth == 0) {
        int score = Evaluate(state);
        return playerColor * score;
    }

    auto moves = generateAllMovesFromState(state, playerColor);

    if (moves.empty()) {
        int player = (playerColor == 1) ? 0 : 1;
        int kingIndex = (player == 0) ? WHITE_KINGS : BLACK_KINGS;
        uint64_t kingBoard = _bitboards[kingIndex];
        int opponent = 1 - player;
        uint64_t danger = computeAttackedSquares(opponent);
        if (kingBoard & danger) {
            return -(9999999 - (MAX_DEPTH - depth));
        }
        return 0;
    }

    int bestVal = -9999999;
    for (const auto& move : moves) {
        state.pushMove(const_cast<BitMove&>(move));
        int val = -negamax(state, depth - 1, -beta, -alpha, -playerColor);
        state.popMove();

        if (val > bestVal) bestVal = val;
        if (bestVal > alpha) alpha = bestVal;
        if (alpha >= beta) break;
    }
    return bestVal;
}

void Chess::updateAI() {

    if (!gameHasAI() || !getCurrentPlayer()->isAIPlayer()) {
        return;
    }

    // Build GameState from current board.
    GameState gs;
    //GameState::stackLevel = 0;
    std::string s = stateString();
    for (int i = 0; i < 64 && i < static_cast<int>(s.size()); ++i) {
        gs.state[i] = s[i];
    }

    int side = getCurrentPlayer()->playerNumber();   
    int playerColor = (side == 0) ? 1 : -1;           

    auto moves = generateAllMovesFromState(gs, playerColor);
    if (moves.empty()) return;

    constexpr int INF = 9999999;
    int bestScore = -INF;
    BitMove bestMove = moves[0];

    auto start = std::chrono::high_resolution_clock::now();

    for (auto& move : moves) {
        gs.pushMove(move);
        int score = -negamax(gs, 3, -INF, INF, -playerColor);
        gs.popMove();
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "AI search time: " << duration.count() << " ms, bestScore = " << bestScore << std::endl;

    // Apply chosen move to the real board using existing logic.
    int srcSquare = bestMove.from;
    int dstSquare = bestMove.to;
    BitHolder& src = getHolderAt(srcSquare & 7, srcSquare / 8);
    BitHolder& dst = getHolderAt(dstSquare & 7, dstSquare / 8);
    Bit* bit = src.bit();
    if (!bit) return;
    dst.dropBitAtPoint(bit, ImVec2(0, 0));
    src.releaseBit();
    bitMovedFromTo(*bit, src, dst);
}
