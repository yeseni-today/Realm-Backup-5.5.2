////////////////////////////////////////////////////////////////////////////
//
// Copyright 2014 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

import Realm

public extension RLMRealm {
  @nonobjc class func schemaVersion(at url: URL, usingEncryptionKey key: Data? = nil) throws -> UInt64 {
    var error: NSError?
    let version = __schemaVersion(at: url, encryptionKey: key, error: &error)
    guard version != RLMNotVersioned else { throw error! }
    return version
  }

  @nonobjc func resolve<Confined>(reference: RLMThreadSafeReference<Confined>) -> Confined? {
    return __resolve(reference as! RLMThreadSafeReference<RLMThreadConfined>) as! Confined?
  }
}

public extension RLMObject {
  /// Swift query convenience functions
  class func objects(where predicateFormat: String, _ args: CVarArg...) -> RLMResults<RLMObject> {
    return objects(with: NSPredicate(format: predicateFormat, arguments: getVaList(args))) as! RLMResults<RLMObject>
  }

  class func objects(in realm: RLMRealm,
                     where predicateFormat: String,
                     _ args: CVarArg...) -> RLMResults<RLMObject>
  {
    return objects(in: realm, with: NSPredicate(format: predicateFormat, arguments: getVaList(args))) as! RLMResults<RLMObject>
  }
}

// MARK: - RLMIterator
public struct RLMIterator<T>: IteratorProtocol {
  private var iteratorBase: NSFastEnumerationIterator

  init(collection: RLMCollection) {
    iteratorBase = NSFastEnumerationIterator(collection)
  }

  public mutating func next() -> T? {
    return iteratorBase.next() as! T?
  }
}

// MARK: - RLMArray + Sequence
/// Sequence conformance for RLMArray and RLMResults is provided by RLMCollection's
/// `makeIterator()` implementation.
extension RLMArray: Sequence {}

// MARK: - RLMResults + Sequence
extension RLMResults: Sequence {}

public extension RLMCollection {
  /// Support Sequence-style enumeration
  func makeIterator() -> RLMIterator<RLMObject> {
    return RLMIterator(collection: self)
  }
}

public extension RLMCollection {
  /// Swift query convenience functions
  func indexOfObject(where predicateFormat: String, _ args: CVarArg...) -> UInt {
    return indexOfObject(with: NSPredicate(format: predicateFormat, arguments: getVaList(args)))
  }

  func objects(where predicateFormat: String, _ args: CVarArg...) -> RLMResults<NSObject> {
    return objects(with: NSPredicate(format: predicateFormat, arguments: getVaList(args))) as! RLMResults<NSObject>
  }
}

// MARK: - Sync-related

#if REALM_ENABLE_SYNC
public extension RLMSyncManager {
  static var shared: RLMSyncManager {
    return __shared()
  }
}

public extension RLMSyncUser {
  static var current: RLMSyncUser? {
    return __current()
  }

  static var all: [String: RLMSyncUser] {
    return __allUsers()
  }

  @nonobjc var errorHandler: RLMUserErrorReportingBlock? {
    get {
      return __errorHandler
    }
    set {
      __errorHandler = newValue
    }
  }

  static func logIn(with credentials: RLMSyncCredentials,
                    server authServerURL: URL,
                    timeout: TimeInterval = 30,
                    callbackQueue queue: DispatchQueue = DispatchQueue.main,
                    onCompletion completion: @escaping RLMUserCompletionBlock)
  {
    return __logIn(with: credentials,
                   authServerURL: authServerURL,
                   timeout: timeout,
                   callbackQueue: queue,
                   onCompletion: completion)
  }

  func configuration(realmURL: URL? = nil, fullSynchronization: Bool = false,
                     enableSSLValidation: Bool = true, urlPrefix: String? = nil) -> RLMRealmConfiguration
  {
    return self.__configuration(with: realmURL,
                                fullSynchronization: fullSynchronization,
                                enableSSLValidation: enableSSLValidation,
                                urlPrefix: urlPrefix)
  }
}

public extension RLMSyncSession {
  func addProgressNotification(for direction: RLMSyncProgressDirection,
                               mode: RLMSyncProgressMode,
                               block: @escaping RLMProgressNotificationBlock) -> RLMProgressNotificationToken?
  {
    return __addProgressNotification(for: direction,
                                     mode: mode,
                                     block: block)
  }
}
#endif
