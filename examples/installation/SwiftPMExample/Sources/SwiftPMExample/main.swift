import Foundation
import RealmSwift

// MARK: - MyModel
class MyModel: Object {
  @objc dynamic var str: String = ""
}

let realm = try! Realm()
try! realm.write {
  realm.create(MyModel.self, value: ["Hello, world!"])
}

print(realm.objects(MyModel.self).last!.str)
