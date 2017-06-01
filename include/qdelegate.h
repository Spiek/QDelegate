#ifndef QDELEGATE_H
#define QDELEGATE_H

// std lib
#include <functional>

// qt
#include <QObject>

#include <QArgument>
#include <QGenericArgument>
#include <QVariant>

// Template to normalize T to Value Type
template< typename T>
struct ValueType
{ typedef T type; };

template< typename T>
struct ValueType<T*>
{  typedef typename std::remove_pointer<T>::type type; };

template< typename T>
struct ValueType<T&>
{  typedef typename std::remove_reference<T>::type type; };


// Template to convert type to Reference type
template<class T>
T& convertToRef(T* p) { return *p; }

template<class T>
T& convertToRef(T& r) { return r; }


// If T is void type, type is std::true_type otherwise std::false_type
template<typename T>
struct IsVoidType
{ typedef std::false_type type; };

template<>
struct IsVoidType<void>
{ typedef std::true_type type; };


// If T is void type, type is void otherwise C
template<typename T, typename C>
struct IfTVoidOtherwiseC
{ typedef C type; };

template<typename C>
struct IfTVoidOtherwiseC<void,C>
{ typedef void type; };


// If T is void type, type is void* otherwise T
template<typename T>
struct UseableType
{ typedef T type; };

template<>
struct UseableType<void>
{ typedef void* type; };


//
// Base Invoker
// and Invoker for function objects
//
template <typename...> class QDelegateInvoker;
template<typename ReturnValue, typename... Args>
class QDelegateInvoker<ReturnValue(Args...)>
{
	public:
		QDelegateInvoker() { }
		virtual ~QDelegateInvoker() { }
		QDelegateInvoker(std::function<ReturnValue(Args...)> functor) : functor(functor) { }
		virtual ReturnValue invoke(Args... args) {
			return this->functor(args...);
		}

	private:
		std::function<ReturnValue(Args...)> functor;
};

//
// Invoker for invoking Object method
//
template<typename Object, typename Method, typename ReturnValue, typename... Args>
class QDelegateInvoker<Object,Method,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
	public:
		QDelegateInvoker(Object* object, Method method) : object(object), method(method) {
			// if no valid object is present, inform user
			if(!this->object) qWarning("QDelegate<Object,Method>(): object is not valid, invoke will fail...");
		}
		virtual ReturnValue invoke(Args... args) override {
			// if no valid object is present, return default constrcuted value
			if(!this->object) {
				qWarning("QDelegate<Object,Method>::invoke: object is not valid, return default constructed value");
				return ReturnValue();
			}
			return std::bind(this->method, this->object, args...)();
		}

	private:
		Object* object = 0;
		Method method;
};

//
// Invoker for invoking QObject method
//
template<typename Placeholder, typename Object, typename Method, typename ReturnValue, typename... Args>
class QDelegateInvoker<Placeholder,Object,Method,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
	public:
		QDelegateInvoker(Object* object, Method method) : object(object), method(method) {
			// if no valid object is present, inform user
			if(!this->object) qWarning("QDelegate<QObject,Method>(): object is not valid, invoke will fail...");
			else this->deleteConnection = QObject::connect(object, &QObject::destroyed, [this] { this->object = 0; });
		}
		~QDelegateInvoker() {
			QObject::disconnect(this->deleteConnection);
		}
		virtual ReturnValue invoke(Args... args) override {
			// if no valid object is present, return default constrcuted value
			if(!this->object) {
				qWarning("QDelegate<QObject>::invoke: object is not valid, return default constructed value");
				return ReturnValue();
			}
			return std::bind(this->method, this->object, args...)();
		}

	private:
		Object* object = 0;
		Method method;
		QMetaObject::Connection deleteConnection;
};


//
// Invoker for invoking QObject method via SIGNAL or SLOT name
//
template<typename ReturnValue, typename... Args>
class QDelegateInvoker<QObject,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
	public:
		QDelegateInvoker(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) : conType(conType), object(object), method(method) {
			this->initialize();
		}
		QDelegateInvoker(QObject* object, QByteArray method, Qt::ConnectionType conType = Qt::DirectConnection) : conType(conType), object(object), method(method) {
			this->initialize();
		}
		~QDelegateInvoker() {
			QObject::disconnect(this->deleteConnection);
		}
		virtual ReturnValue invoke(Args... args) override {
			return this->invokeHelper(this->isReturnTypeVoid, args...);
		}

	private:
		// invoker for void return value
		ReturnValue invokeHelper(std::true_type const &, Args... args)
		{
			// if no valid object is present, exit
			if(!this->object) {
				qWarning("QDelegate<QObject,QByteArray>: object is not valid, return default constructed value");
				return;
			}

			// invoke
			QMetaObject* metaObject = this->object->metaObject();
			if(!metaObject->invokeMethod(this->object,
										 this->method,
										 this->conType,
										 QArgument<Args>(QVariant(args).typeName(), args)...))
			{
				qWarning("QDelegate<QObject,QByteArray>: invoke failed (Object: %s, Method: %s)", this->object->metaObject()->className(), this->method.isEmpty() ? "{empty}" : this->method.data());
			}
		}

		// invoker for non void return value
		ReturnValue invokeHelper(std::false_type const &, Args... args)
		{
			// if no valid object is present, exit with default value
			ReturnValue retValue = {};
			if(!this->object) {
				qWarning("QDelegate<QObject,QByteArray>: object is not valid, return default constructed value");
				return retValue;
			}

			// Queued Invoke: do an invoke without ReturnValue, and return the default value
			QMetaObject* metaObject = this->object->metaObject();
			if(this->conType == Qt::QueuedConnection)
			{
				if(metaObject->invokeMethod(this->object,
											this->method,
											this->conType,
											QArgument<Args>(QVariant(args).typeName(), args)...))
				{
					return retValue;
				}
			}

			// Non Queued invoke: invoke and return retValue on success
			else {
				if(metaObject->invokeMethod(this->object,
											this->method,
											this->conType,
											QReturnArgument<ReturnValue>(QVariant(retValue).typeName(), convertToRef<ReturnValue>(retValue)),
											QArgument<Args>(QVariant(args).typeName(), args)...))
				{
					return retValue;
				}
		   }

			// if we reach this point there was an invoke error, so show warning and return default value
			qWarning("QDelegate<QObject,QByteArray>: invoke failed (Object: %s, Method: %s)", this->object->metaObject()->className(), this->method.isEmpty() ? "{empty}" : this->method.data());
			return retValue;
		}

		void initialize()
		{
			// if no valid object is present, inform user
			if(!this->object) {
				qWarning("QDelegate<QObject,QByteArray>(): object is not valid, invoke will fail...");
				return;
			}

			// init SEGFAULT protection: if object is destroyed, set object to 0
			this->deleteConnection = QObject::connect(object, &QObject::destroyed, [this] { this->object = 0; });

			// normalize signature
			this->method = QMetaObject::normalizedSignature(this->method);

			// remove method brackets and (if present) the signal/slot identifier
			// Note:    code base taken from qt source (4.8.2)
			//          src: qtimer.cpp
			//          line: 354
			const char* rawMethodName = this->method.data();
			const char* bracketPosition = strchr(this->method.data(), '(');
			if(bracketPosition) {
				bool hasSignalSlotIndetifier = (*rawMethodName >= '0' && *rawMethodName <= '3');
				this->method = QByteArray(rawMethodName + hasSignalSlotIndetifier, bracketPosition - hasSignalSlotIndetifier - rawMethodName);
			}
		}

		// data
		typename IsVoidType<typename ValueType<ReturnValue>::type>::type isReturnTypeVoid;
		Qt::ConnectionType conType;
		QObject* object = 0;
		QByteArray method;
		QMetaObject::Connection deleteConnection;
};

//
// Invoker for invoking static method
//
template<typename Method, typename ReturnValue, typename... Args>
class QDelegateInvoker<Method,ReturnValue(Args...)> : public QDelegateInvoker<ReturnValue(Args...)>
{
	public:
		QDelegateInvoker(Method method) : functor(method) { }
		virtual ReturnValue invoke(Args... args) override {
			return this->functor(args...);
		}

	private:
		std::function<ReturnValue(Args...)> functor;
};

template <typename...> class QDelegate;
template<typename ReturnValue, typename... Args>
class QDelegate<ReturnValue(Args...)>
{
	public:
		// Constructor: default
		QDelegate() { }

		// Constructor: copy
		QDelegate(const QDelegate<ReturnValue(Args...)>& other) {
			this->invokers = other.invokers;
		}

		// Constructor: function object
		QDelegate(std::function<ReturnValue(Args...)> functor) {
			this->addInvoke(functor);
		}

		// Constructor: static function
		QDelegate(ReturnValue (*method)(Args...)){
			this->addInvoke(method);
		}

		// Constructor: function on Object
		template<class Object, typename = std::enable_if_t<!std::is_base_of<QObject, typename ValueType<Object>::type>::value> >
		QDelegate(Object* object, ReturnValue (Object::*method)(Args...)) {
			this->addInvoke(object, method);
		}

		// Constructor: function on QObject
		template<class Object, typename = std::enable_if_t<std::is_base_of<QObject, typename ValueType<Object>::type>::value> >
		QDelegate(QObject* object, ReturnValue (Object::*method)(Args...)) {
			this->addInvoke((Object*)object, method);
		}

		// Constructor: const char* function on QObject
		QDelegate(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) {
			this->addInvoke(object, method, conType);
		}

		// Constructor: QBytearray function on QObject
		QDelegate(QObject* object, QByteArray method, Qt::ConnectionType conType = Qt::DirectConnection) {
			this->addInvoke(object, method, conType);
		}

		// addInvoke: QDelegate
		QDelegate<ReturnValue(Args...)>& addInvoke(QDelegate<ReturnValue(Args...)> delegate) {
			this->invokers.append(delegate.invokers);
			return *this;
		}

		// addInvoke: function object
		QDelegate<ReturnValue(Args...)>& addInvoke(std::function<ReturnValue(Args...)> functor) {
			this->invokers.append(QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>(new QDelegateInvoker<ReturnValue(Args...)>(functor)));
			return *this;
		}

		// addInvoke: static function
		QDelegate<ReturnValue(Args...)>& addInvoke(ReturnValue (*method)(Args...))  {
			this->invokers.append(QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>(new QDelegateInvoker<ReturnValue (*)(Args...),ReturnValue(Args...)>(method)));
			return *this;
		}

		// addInvoke: function on Object
		template<class Object, typename = std::enable_if_t<!std::is_base_of<QObject, typename ValueType<Object>::type>::value> >
		QDelegate<ReturnValue(Args...)>& addInvoke(Object* object, ReturnValue (Object::*method)(Args...)) {
			// object check
			if(!object) {
				qWarning("QDelegate<Object>: object is not valid, object is not invokable!");
				return *this;
			}
			this->invokers.append(QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>(new QDelegateInvoker<Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>(object, method)));
			return *this;
		}

		// addInvoke: function on QObject
		template<class Object, typename = std::enable_if_t<std::is_base_of<QObject, typename ValueType<Object>::type>::value> >
		QDelegate<ReturnValue(Args...)>& addInvoke(QObject* object, ReturnValue (Object::*method)(Args...)) {
			// object check
			if(!object) {
				qWarning("QDelegate<QObject>: object is not valid, object is not invokable!");
				return *this;
			}
			this->invokers.append(QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>(new QDelegateInvoker<void,Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>((Object*)object, method)));
			return *this;
		}

		// addInvoke: const char* function on QObject
		QDelegate<ReturnValue(Args...)>& addInvoke(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) {
			// object check
			if(!object) {
				qWarning("QDelegate<QObject,const char*>: object is not valid, object is not invokable!");
				return *this;
			}
			this->invokers.append(QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>(new QDelegateInvoker<QObject,ReturnValue(Args...)>(object, method, conType)));
			return *this;
		}

		// addInvoke: QBytearray function on QObject
		QDelegate<ReturnValue(Args...)>& addInvoke(QObject* object, QByteArray method, Qt::ConnectionType conType = Qt::DirectConnection) {
			// object check
			if(!object) {
				qWarning("QDelegate<QObject,QByteArray>: object is not valid, object is not invokable!");
				return *this;
			}
			this->invokers.append(QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>(new QDelegateInvoker<QObject,ReturnValue(Args...)>(object, method, conType)));
			return *this;
		}

		// operators
		QDelegate<ReturnValue(Args...)>& operator=(QDelegate<ReturnValue(Args...)> &&other) {
			this->invokers = other.invokers;
			return *this;
		}

		// full invoke
		typename IfTVoidOtherwiseC<ReturnValue, QList<ReturnValue>>::type
		invoke(Args... args) {
			return this->invokeHelper(this->isReturnTypeVoid, args...);
		}

		// fast invoke
		inline void fastInvoke(Args... args) {
			this->invokeHelper(this->typeTrue, args...);
		}

	private:
		// invokeHelper for void type
		void invokeHelper(std::true_type, Args... args) {
			for(int i = 0; i < this->invokers.count(); i++) {
				// if invoker is not valid, skip it
				auto& invoker = this->invokers.at(i);
				if(!invoker) {
					qWarning("QDelegate::invokeAll: No valid invoker available, skip");
					continue;
				}

				// invoke
				invoker->invoke(args...);
			}
		}

		// invokeHelper for non void type
		QList<typename UseableType<ReturnValue>::type> invokeHelper(std::false_type, Args... args) {
			QList<typename UseableType<ReturnValue>::type> returnValues;
			for(int i = 0; i < this->invokers.count(); i++) {
				// if invoker is not valid, skip it
				auto& invoker = this->invokers.at(i);
				if(!invoker) {
					qWarning("QDelegate::invokeAll: No valid invoker available, skip");
					continue;
				}

				// invoke
				returnValues.append(invoker->invoke(args...));
			}
			return returnValues;
		}

		// data
		typename IsVoidType<typename ValueType<ReturnValue>::type>::type isReturnTypeVoid;
		const static std::true_type typeTrue;
		QList<QSharedPointer<QDelegateInvoker<ReturnValue(Args...)>>> invokers;
};

#endif // QDELEGATE_H
