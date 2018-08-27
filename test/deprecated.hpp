////////////////////////////////////////////////////////////////////////////////

namespace Event {
	enum Enum : unsigned {
		GUARD,
		ENTER,
		UPDATE,
		REACT_REQUEST,
		REACT,
		EXIT,

		RESTART,
		RESUME,
		SCHEDULE,

		COUNT
	};
};

//------------------------------------------------------------------------------

struct Status {
	std::type_index state;
	Event::Enum func;

	inline bool operator == (const Status& reference) const {
		return func == reference.func && state == reference.state;
	}
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template <typename T>
Status status(Event::Enum event) {
	using Type = T;

	return Status{ std::type_index(typeid(Type)), event };
}

//------------------------------------------------------------------------------

struct Context {
	using History = std::vector<Status>;

	template <unsigned NCapacity>
	void assertHistory(const Status (&reference)[NCapacity]) {
		const auto historySize = (unsigned) history.size();
		const auto referenceSize = hfsm::detail::count(reference);

		const auto size = std::min(historySize, referenceSize);

		for (unsigned i = 0; i < size; ++i) {
			HSFM_IF_ASSERT(const auto h = history[i]);
			HSFM_IF_ASSERT(const auto r = reference[i]);
			assert(h == r);
		}
		assert(historySize == referenceSize);

		history.clear();
	}

	float deltaTime = 0.0f;

	History history;
};
using M = hfsm::Machine<Context>;

//------------------------------------------------------------------------------

#define S(s) struct s

using FSM = M::PeerRoot<
				M::Composite<S(A),
					S(A_1),
					M::Composite<S(A_2),
						S(A_2_1),
						S(A_2_2)
					>
				>,
				M::Orthogonal<S(B),
					M::Composite<S(B_1),
						S(B_1_1),
						S(B_1_2)
					>,
					M::Composite<S(B_2),
						S(B_2_1),
						S(B_2_2)
					>
				>
			>;

#undef S

//------------------------------------------------------------------------------

static_assert(FSM::stateId<A>()		==  1, "");
static_assert(FSM::stateId<A_1>()	==  2, "");
static_assert(FSM::stateId<A_2>()	==  3, "");
static_assert(FSM::stateId<A_2_1>()	==  4, "");
static_assert(FSM::stateId<A_2_2>()	==  5, "");
static_assert(FSM::stateId<B>()		==  6, "");
static_assert(FSM::stateId<B_1>()	==  7, "");
static_assert(FSM::stateId<B_1_1>()	==  8, "");
static_assert(FSM::stateId<B_1_2>()	==  9, "");
static_assert(FSM::stateId<B_2>()	== 10, "");
static_assert(FSM::stateId<B_2_1>()	== 11, "");
static_assert(FSM::stateId<B_2_2>()	== 12, "");

////////////////////////////////////////////////////////////////////////////////

class Timed
	: public FSM::Bare
{
public:
	void preEnter(Context&)		{ _elapsed = 0.0f;			}
	void preUpdate(Context& _)	{ _elapsed += _.deltaTime;	}

	float elapsed() const		{ return _elapsed;			}

private:
	float _elapsed;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

class Tracked
	: public FSM::Bare
{
public:
	void preEnter(Context&) {
		++_entryCount;
		_currentUpdateCount = 0;
	}

	void preUpdate(Context&) {
		++_currentUpdateCount;
		++_totalUpdateCount;
	}

	unsigned entryCount() const			{ return _entryCount;			}
	unsigned currentUpdateCount() const { return _currentUpdateCount;	}
	unsigned totalUpdateCount() const	{ return _totalUpdateCount;		}

private:
	unsigned _entryCount = 0;
	unsigned _currentUpdateCount = 0;
	unsigned _totalUpdateCount = 0;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct Action {};

template <typename T>
struct HistoryBase
	: FSM::Bare
{
	void preGuard(Context& _) const {
		_.history.push_back(Status{ typeid(T), Event::GUARD });
	}

	void preEnter(Context& _) {
		_.history.push_back(Status{ typeid(T), Event::ENTER });
	}

	void preUpdate(Context& _) {
		_.history.push_back(Status{ typeid(T), Event::UPDATE });
	}

	void preReact(const Action&, Context& _) {
		_.history.push_back(Status{ typeid(T), Event::REACT_REQUEST });
	}

	void postExit(Context& _) {
		_.history.push_back(Status{ typeid(T), Event::EXIT });
	}
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template <typename T>
using State = FSM::BaseT<Tracked, Timed, HistoryBase<T>>;

//------------------------------------------------------------------------------

template <typename T>
void
changeTo(FSM::TransitionControl& control, Context::History& history) {
	control.template changeTo<T>();
	history.push_back(Status{ typeid(T), Event::RESTART });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template <typename T>
void
resume(FSM::TransitionControl& control, Context::History& history) {
	control.template resume<T>();
	history.push_back(Status{ typeid(T), Event::RESUME });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

template <typename T>
void
schedule(FSM::TransitionControl& control, Context::History& history) {
	control.template schedule<T>();
	history.push_back(Status{ typeid(T), Event::SCHEDULE });
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct Reacting
	: State<T>
{
	void react(const Action&, FSM::TransitionControl& control) {
		control._().history.push_back(Status{ typeid(T), Event::REACT });
	}
};

////////////////////////////////////////////////////////////////////////////////

struct A : Reacting<A> {};

//------------------------------------------------------------------------------

struct A_1
	: State<A_1>
{
	void update(TransitionControl& control) {
		changeTo<A_2>(control, control._().history);
	}
};

//------------------------------------------------------------------------------

struct A_2
	: State<A_2>
{
	void update(TransitionControl& control) {
		switch (entryCount()) {
		case 1:
			changeTo<B_2_2>(control, control._().history);
			break;

		case 2:
			resume<B>(control, control._().history);
			break;
		}
	}
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct A_2_1 : Reacting<A_2_1> {};
struct A_2_2 : Reacting<A_2_2> {};

//------------------------------------------------------------------------------

struct B	 : Reacting<B> {};

struct B_1	 : State<B_1> {};
struct B_1_1 : State<B_1_1> {};
struct B_1_2 : State<B_1_2> {};

struct B_2	 : State<B_2> {};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct B_2_1
	: State<B_2_1>
{
	void guard(TransitionControl& control) {
		resume<B_2_2>(control, control._().history);
	}
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct B_2_2
	: State<B_2_2>
{
	void guard(TransitionControl& control) {
		if (entryCount() == 2)
			control.resume<A>();
	}

	void update(TransitionControl& control) {
		switch (totalUpdateCount()) {
		case 1:
			resume<A>(control, control._().history);
			break;

		case 2:
			changeTo<B>(control, control._().history);
			break;

		case 3:
			schedule<A_2_2>(control, control._().history);
			resume<A>(control, control._().history);
			break;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////

static_assert(FSM::Instance::DEEP_WIDTH	 ==  2, "");
static_assert(FSM::Instance::STATE_COUNT == 13, "");
static_assert(FSM::Instance::COMPO_COUNT ==  5, "");
static_assert(FSM::Instance::ORTHO_COUNT ==  1, "");
static_assert(FSM::Instance::ORTHO_UNITS ==  1, "");
static_assert(FSM::Instance::PRONG_COUNT == 10, "");

////////////////////////////////////////////////////////////////////////////////