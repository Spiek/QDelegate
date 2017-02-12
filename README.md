# QDelegate

QDelegate provides easy to use delegates for:
* Functors
* Functions
* Class Functions
* Object functions
* QObject SIGNALS/SLOTS

### Standard Invokes

```c++
  // static functions
  QDelegate<int(int, int)>(&MyObject::staticFunction).invoke(10, 1);
  QDelegate<int(int, int)>(&staticFunction).invoke(10, 2);

  // function objects
  QDelegate<int(int, int)>([](int a, int b){ return a + b; }).invoke(10, 3);

  // dynamic objects
  QDelegate<int(int, int)>(MyObject, &MyObject::myfunction).invoke(10, 4);
```
Additional Notes to QObject QDelegates:
* SEGFAULT protection: QDelegate automaticly detects deletions of QObjects

### QObject Invokes

if QDelegate is used to invoke a method on a QObject, there are a few **more** possible invoke options:
```c++
  // invoke a QObject function via functionname as QByteArray or const char*
  QDelegate<int(int, int)>(QObject, "myfunction", Qt::DirectConnection).invoke(10, 5);
  
  // invoke a QObject function via SIGNAL/SLOT macro
  QDelegate<int(int, int)>(QObject, SIGNAL(mysignal(int, int)), Qt::DirectConnection).invoke(10, 7);
  QDelegate<int(int, int)>(QObject, SLOT(myslot(int, int)), Qt::DirectConnection).invoke(10, 6);
```
Additional Notes to QObject QDelegates:
* Function names provided as string are normalized, meaning the following function syntaxes are possible: "function()", "function"
* If you set the Qt::ConnectionType to Qt::QueuedConnection invoke will return a default constructed value of the return value and invoke the function async!
* SEGFAULT protection: QDelegate automaticly detects deletions of QObjects