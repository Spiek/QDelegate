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
        QDelegateInvoker(Object* object, Method method) : object(object), method(method) { }
        virtual ReturnValue invoke(Args... args) override {
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
            this->deleteConnection = QObject::connect(object, &QObject::destroyed, [this] { this->object = 0; });
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
            typename IsVoidType<ReturnValue>::type vType;
            return this->invokeHelper(vType, args...);
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
            if(!this->object->metaObject()->invokeMethod(this->object,
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
            if(this->conType == Qt::QueuedConnection)
            {
                if(this->object->metaObject()->invokeMethod(this->object,
                                                            this->method,
                                                            this->conType,
                                                            QArgument<Args>(QVariant(args).typeName(), args)...))
                {
                    return retValue;
                }
            }

            // Non Queued invoke: invoke and return retValue on success
            else {
                if(this->object->metaObject()->invokeMethod(this->object,
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
        QDelegateInvoker(Method method) : method(method) { }
        virtual ReturnValue invoke(Args... args) override {
            return std::bind(this->method, args...)();
        }

    private:
        Method method;
};

template <typename...> class QDelegate;
template<typename ReturnValue, typename... Args>
class QDelegate<ReturnValue(Args...)>
{
    public:
        QDelegate(std::function<ReturnValue(Args...)> functor) {
            this->invoker = new QDelegateInvoker<ReturnValue(Args...)>(functor);
        }

        QDelegate(ReturnValue (*method)(Args...))  {
            this->invoker = new QDelegateInvoker<ReturnValue (*)(Args...),ReturnValue(Args...)>(method);
        }

        template<typename Object, typename = std::enable_if_t<!std::is_base_of<QObject, typename ValueType<Object>::type>::value, Object>>
        QDelegate(Object* object, ReturnValue (Object::*method)(Args...)) {
            // object check
            if(!object) {
                qWarning("QDelegate<Object>: object is not valid, object is not invokable!");
                return;
            }
            this->invoker = new QDelegateInvoker<Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>(object, method);
        }

        template<typename Object, typename = std::enable_if_t<std::is_base_of<QObject, typename ValueType<Object>::type>::value, Object>>
        QDelegate(QObject* object, ReturnValue (Object::*method)(Args...)) {
            // object check
            if(!object) {
                qWarning("QDelegate<QObject>: object is not valid, object is not invokable!");
                return;
            }
            this->invoker = new QDelegateInvoker<void,Object,ReturnValue (Object::*)(Args...),ReturnValue(Args...)>((Object*)object, method);
        }

        QDelegate(QObject* object, const char* method, Qt::ConnectionType conType = Qt::DirectConnection) {
            // object check
            if(!object) {
                qWarning("QDelegate<QObject,const char*>: object is not valid, object is not invokable!");
                return;
            }
            this->invoker = new QDelegateInvoker<QObject,ReturnValue(Args...)>(object, method, conType);
        }

        QDelegate(QObject* object, QByteArray method, Qt::ConnectionType conType = Qt::DirectConnection) {
            // object check
            if(!object) {
                qWarning("QDelegate<QObject,QByteArray>: object is not valid, object is not invokable!");
                return;
            }
            this->invoker = new QDelegateInvoker<QObject,ReturnValue(Args...)>(object, method, conType);
        }

        ~QDelegate() {
            delete this->invoker;
        }

        ReturnValue invoke(Args... args) {
            // if no invoker is present, return default constrcuted value
            if(!this->invoker) {
                qWarning("QDelegate::invoke: No valid invoker available, return default constrcuted value");
                return ReturnValue();
            }

            // otherwise call invoker
            return this->invoker->invoke(args...);
        }

    private:
         QDelegateInvoker<ReturnValue(Args...)>* invoker = 0;
};

#endif // QDELEGATE_H
