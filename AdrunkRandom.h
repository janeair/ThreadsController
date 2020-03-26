#ifndef ADRUNK_RANDOM_H
#define ADRUNK_RANDOM_H

#include "qglobal.h"
#include "qvector.h"
#include "qbitarray.h"

#define N_CARDS 36
#define N_SUITS 4

class AdrunkRandom
{

public:
	AdrunkRandom() { moves = 0; left.clear(); right.clear(); }
	enum Card { Card6, Card7, Card8, Card9, Card10, CardJack, CardQueen, CardKing, CardAce };
	enum CardRelation { LeftWin, Equal, RightWin };
	enum MoveResult { LeftWinMove, RightWinMove, LeftWinGame, RightWinGame, UnknownResult };

	inline bool getBit();
	inline QBitArray& getBits(quint8 bits);
	quint32& getMoves() { return moves; }
	inline QList<QString>& getState();
	inline bool init();
	inline bool init(QVector<Card> one, QVector<Card> two);
	
private:
	QVector<Card> left;
	QVector<Card> right;
	quint32 moves;

	inline MoveResult move();
	inline CardRelation compare(Card left, Card right);
	inline bool check(QVector<Card> deck);
	inline QString& getCardString(Card card);
	inline QString& getLeftDeckString();
	inline QString& getRightDeckString();
};

// PRIVATE METHODS

AdrunkRandom::MoveResult AdrunkRandom::move()
{
	QVector<Card> temp_deck = QVector<Card>();
	moves++;

	do {
		if (!right.isEmpty()) temp_deck.push_front(right.takeFirst());
		else return LeftWinGame;
		if (!left.isEmpty()) temp_deck.push_front(left.takeFirst());
		else return RightWinGame;
		switch (compare(temp_deck.at(0), temp_deck.at(1))) {
		case LeftWin:
		{
						left << temp_deck;
						return LeftWinMove;
						break;
		}
		case RightWin:
		{
						 right << temp_deck;
						 return RightWinMove;
						 break;
		}
		case Equal:
		{
					  if (!right.isEmpty()) temp_deck.push_front(right.takeFirst());
					  else return LeftWinGame;
					  if (!left.isEmpty()) temp_deck.push_front(left.takeFirst());
					  else return RightWinGame;
					  break;
		}
		default:
			return UnknownResult;
		}
	} while (true);
	return UnknownResult;
}

AdrunkRandom::CardRelation AdrunkRandom::compare(Card left, Card right)
{
	if (left == right) {
		return Equal;
	}
	else if ((left > right && !(left == CardAce && right == Card6)) || (left == Card6 && right == CardAce)) {
		return LeftWin;
	}
	else { return RightWin; }
}

bool AdrunkRandom::check(QVector<Card> deck)
{
	if (deck.length != N_CARDS)
	{
		return false;
	}
	else
	{
		if (deck.count(Card6) == N_SUITS 
			&& deck.count(Card7) == N_SUITS
			&& deck.count(Card8) == N_SUITS
			&& deck.count(Card9) == N_SUITS
			&& deck.count(Card10) == N_SUITS
			&& deck.count(CardJack) == N_SUITS
			&& deck.count(CardQueen) == N_SUITS
			&& deck.count(CardKing) == N_SUITS 
			&& deck.count(CardAce) == N_SUITS)
			return true;
		else
			return false;
	}
}

QString& AdrunkRandom::getCardString(Card card)
{
	QString card_name = QString();
	switch (card) {
	case Card6: { card_name = "6"; break; }
	case Card7: { card_name = "7"; break; }
	case Card8: { card_name = "8"; break; }
	case Card9: { card_name = "9"; break; }
	case Card10: { card_name = "10"; break; }
	case CardJack: { card_name = "J"; break; }
	case CardQueen: { card_name = "Q"; break; }
	case CardKing: { card_name = "K"; break; }
	case CardAce: { card_name = "A"; break; }
	default: {card_name = "Unknown"; break; }
	}
	return card_name;
}

QString& AdrunkRandom::getLeftDeckString()
{
	QString left_deck = QString("left: ");
	foreach(Card card, left)
		left_deck += getCardString(card);
	return left_deck;
}

QString& AdrunkRandom::getRightDeckString()
{
	QString right_deck = QString("right: ");
	foreach(Card card, right)
		right_deck += getCardString(card);
	return right_deck;
}

// PUBLIC METHODS

bool AdrunkRandom::getBit()
{
	switch (move()) {
	case LeftWinMove:
		return true;
	case RightWinMove:
		return false;
	case LeftWinGame:
	{
		init();
		return true;
	}
	case RightWinGame:
	{
		init();
		return false;
	}
	case UnknownResult:
	{
		init();
		return getBit();
	}
	}
}

QBitArray& AdrunkRandom::getBits(quint8 bits)
{
	QBitArray move_sequence = QBitArray(bits);
	for (quint8 i = 0; i < bits; i++)
		move_sequence.setBit(i, getBit());
	return move_sequence;
}

QList<QString>& AdrunkRandom::getState()
{
	QList<QString> deck_state = QList<QString>();
	QString moves_state = QString("moves: "); moves_state += QString::number(moves);
	deck_state.append(moves_state);
	deck_state.append(getLeftDeckString());
	deck_state.append(getRightDeckString());
	return deck_state;
}

bool AdrunkRandom::init()
{

}

bool AdrunkRandom::init(QVector<Card> one, QVector<Card> two)
{
	if (check(one + two))
	{
		left = one;
		right = two;
		moves = 0;
		return true;
	}
	else return false;
}


#endif // ADRUNK_RANDOM_H