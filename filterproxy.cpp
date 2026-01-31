#include "filterproxy.h"

#include "thumbnailmodel.h"

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVector>
#include <QStack>
#include <functional>


namespace QueryMatcher {

// ---------- Lexing ----------

enum class LexKind { Word, Number, CmpOp, AndOp, OrOp };

struct LexToken {
    LexKind kind;
    QString text;   // for Word/CmpOp
    qint64 number{}; // for Number
};

static bool isDelim(QChar c) {
    return c.isSpace() || c == '&' || c == '|' || c == '<' || c == '>' || c == '=';
}

static bool isAllDigits(const QString& s) {
    if (s.isEmpty()) return false;
    for (QChar c : s) if (!c.isDigit()) return false;
    return true;
}

static QVector<LexToken> lex(const QString& input) {
    QVector<LexToken> out;
    const QString s = input.trimmed();
    int i = 0;

    auto peek = [&](int off = 0) -> QChar {
        const int k = i + off;
        return (k >= 0 && k < s.size()) ? s[k] : QChar();
    };

    while (i < s.size()) {
        const QChar c = s[i];

        if (c.isSpace()) { ++i; continue; }

        if (c == '&') { out.push_back({LexKind::AndOp, "&", 0}); ++i; continue; }
        if (c == '|') { out.push_back({LexKind::OrOp,  "|", 0}); ++i; continue; }

        // Comparison operators: <= >= == < >
        if (c == '<' || c == '>' || c == '=') {
            QString op;
            op += c;
            if ((c == '<' || c == '>') && peek(1) == '=') { op += '='; i += 2; }
            else if (c == '=' && peek(1) == '=') { op = "=="; i += 2; }
            else { ++i; } // allows single '<' or '>' or (oddly) single '='

            out.push_back({LexKind::CmpOp, op, 0});
            continue;
        }

        // Read a "wordish" token until delimiter (allows dots, dashes, unicode, etc.)
        int start = i;
        while (i < s.size() && !isDelim(s[i])) ++i;
        const QString token = s.mid(start, i - start);

        if (isAllDigits(token)) {
            out.push_back({LexKind::Number, QString(), token.toLongLong()});
        } else {
            out.push_back({LexKind::Word, token, 0});
        }
    }

    return out;
}

// ---------- Predicates / Compilation ----------

enum class OpKind { And, Or };

struct ProgTok {
    // If pred is set -> predicate token; else operator token
    std::function<bool(const FileItem&)> pred;
    OpKind op{};
    bool isPred = false;
};

static int precedence(OpKind op) {
    // & has higher precedence than |
    return (op == OpKind::And) ? 2 : 1;
}

static bool ciEquals(const QString& a, const QString& b) {
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

static bool tagsContainCI(const QStringList& tags, const QString& needle) {
    for (const QString& t : tags) {
        if (ciEquals(t, needle)) return true;
    }
    return false;
}

static std::function<bool(const FileItem&)> makeUnaryPredicate(const QString& rawToken) {
    const QString tok = rawToken.trimmed();
    const QString low = tok.toLower();

    if (low == "picture") {
        return [](const FileItem& it){ return it.kind == FileKind::Picture; };
    }
    if (low == "video") {
        return [](const FileItem& it){ return it.kind == FileKind::Video; };
    }

    // Default unary: filename contains OR tag equals (case-insensitive)
    return [tok](const FileItem& it){
        if (it.fileName.contains(tok, Qt::CaseInsensitive)) return true;
        if (tagsContainCI(it.tags, tok)) return true;
        return false;
    };
}

static bool evalCmp(qint64 left, const QString& op, qint64 right) {
    if (op == "<")  return left <  right;
    if (op == "<=") return left <= right;
    if (op == ">")  return left >  right;
    if (op == ">=") return left >= right;
    if (op == "==") return left == right;
    // If somebody typed weird stuff, fail closed.
    return false;
}

static std::function<bool(const FileItem&)> makeBinaryPredicate(
    const QString& fieldRaw, const QString& cmpOp, qint64 value)
{
    const QString field = fieldRaw.toLower();

    if (field == "year") {
        return [cmpOp, value](const FileItem& it){
            const int y = it.modified.isValid() ? it.modified.date().year() : 0;
            return evalCmp(y, cmpOp, value);
        };
    }

    if (field == "size") {
        return [cmpOp, value](const FileItem& it){
            return evalCmp(it.sizeBytes, cmpOp, value);
        };
    }

    // Unknown field => never matches
    return [](const FileItem&){ return false; };
}

// Convert lex tokens -> infix predicate/operator stream, inserting implicit AND
static QVector<ProgTok> toInfix(const QVector<LexToken>& ltoks) {
    QVector<ProgTok> infix;

    auto pushOp = [&](OpKind op){
        infix.push_back(ProgTok{std::function<bool(const FileItem&)>(), op, false});
    };
    auto pushPred = [&](std::function<bool(const FileItem&)> p){
        ProgTok t;
        t.pred = std::move(p);
        t.isPred = true;
        infix.push_back(std::move(t));
    };
    auto isPrevPred = [&](){
        return !infix.isEmpty() && infix.back().isPred;
    };

    for (int i = 0; i < ltoks.size(); ++i) {
        const LexToken& t = ltoks[i];

        if (t.kind == LexKind::AndOp) { pushOp(OpKind::And); continue; }
        if (t.kind == LexKind::OrOp)  { pushOp(OpKind::Or);  continue; }

        // Try to build (field cmp number): Word + CmpOp + Number
        if (t.kind == LexKind::Word
            && i + 2 < ltoks.size()
            && ltoks[i + 1].kind == LexKind::CmpOp
            && ltoks[i + 2].kind == LexKind::Number)
        {
            if (isPrevPred()) pushOp(OpKind::And); // implicit AND: "... <pred> year > 2021"
            pushPred(makeBinaryPredicate(t.text, ltoks[i + 1].text, ltoks[i + 2].number));
            i += 2;
            continue;
        }

        // Unary token: Word or Number (numbers treated like text tokens too)
        if (t.kind == LexKind::Word) {
            if (isPrevPred()) pushOp(OpKind::And); // implicit AND on whitespace adjacency
            pushPred(makeUnaryPredicate(t.text));
            continue;
        }

        if (t.kind == LexKind::Number) {
            if (isPrevPred()) pushOp(OpKind::And);
            pushPred(makeUnaryPredicate(QString::number(t.number)));
            continue;
        }

        // Lone comparison operator etc. => ignored (could also make whole query invalid)
        // We'll just ignore it to avoid exploding on typos.
    }

    return infix;
}

// Shunting-yard: infix -> RPN (only binary ops, left-associative)
static QVector<ProgTok> toRpn(const QVector<ProgTok>& infix, bool* okOut) {
    QVector<ProgTok> output;
    QStack<ProgTok> ops;
    bool ok = true;

    for (const ProgTok& t : infix) {
        if (t.isPred) {
            output.push_back(t);
        } else {
            // operator
            while (!ops.isEmpty() && !ops.top().isPred
                   && precedence(ops.top().op) >= precedence(t.op))
            {
                output.push_back(ops.pop());
            }
            ops.push(t);
        }
    }
    while (!ops.isEmpty()) output.push_back(ops.pop());

    // Basic sanity: RPN should be evaluatable
    int stackDepth = 0;
    for (const ProgTok& t : output) {
        if (t.isPred) {
            ++stackDepth;
        } else {
            // binary operator needs 2 operands
            if (stackDepth < 2) { ok = false; break; }
            --stackDepth; // consumes 2, produces 1
        }
    }
    if (stackDepth != 1 && !output.isEmpty()) ok = false;

    if (okOut) *okOut = ok;
    return output;
}

// ---------- Evaluation ----------

static bool evalRpn(const QVector<ProgTok>& rpn, const FileItem& item, bool* okOut) {
    if (rpn.isEmpty()) { if (okOut) *okOut = true; return true; } // empty query matches all

    QStack<bool> st;
    bool ok = true;

    for (const ProgTok& t : rpn) {
        if (t.isPred) {
            st.push(t.pred ? t.pred(item) : false);
        } else {
            if (st.size() < 2) { ok = false; break; }
            const bool b = st.pop();
            const bool a = st.pop();
            const bool r = (t.op == OpKind::And) ? (a && b) : (a || b);
            st.push(r);
        }
    }

    if (st.size() != 1) ok = false;
    if (okOut) *okOut = ok;
    return ok ? st.top() : false;
}

// Public API
inline bool matches(const FileItem& item, const QString& query) {
    const QString q = query.trimmed();
    if (q.isEmpty()) return true;

    const QVector<LexToken> lt = lex(q);
    const QVector<ProgTok> infix = toInfix(lt);

    bool okCompile = true;
    const QVector<ProgTok> rpn = toRpn(infix, &okCompile);

    bool okEval = true;
    const bool result = evalRpn(rpn, item, &okEval);

    // If user typed nonsense, do something predictable: match by raw text as a unary token.
    if (!okCompile || !okEval) {
        return makeUnaryPredicate(q)(item);
    }

    return result;
}

} // namespace QueryMatcher



FilterProxy::FilterProxy(QObject* parent) : QSortFilterProxyModel(parent) {
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FilterProxy::setNeedle(const QString& text) {
    m_needle = text.trimmed();
    invalidateFilter();
}

bool FilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (m_needle.isEmpty()) return true;

    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    const QString name = sourceModel()->data(idx, ThumbnailModel::FileNameRole).toString();
    const QDateTime modified = sourceModel()->data(idx, ThumbnailModel::ModifiedRole).toDateTime();
    const qint64 size = sourceModel()->data(idx, ThumbnailModel::SizeRole).toLongLong();
    const QStringList tags = sourceModel()->data(idx, ThumbnailModel::TagsRole).toStringList();
    const FileKind kind = static_cast<FileKind>(sourceModel()->data(idx, ThumbnailModel::FileKindRole).toInt());

    auto item = FileItem();
    item.fileName = name;
    item.modified = modified;
    item.sizeBytes = size;
    item.tags = tags;
    item.kind = kind;

    return QueryMatcher::matches(item, m_needle);
}

