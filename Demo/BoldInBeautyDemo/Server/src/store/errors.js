// Thrown by a store when a unique value (e-mail, username, id) is already taken.
export class StoreConflict extends Error {
  constructor(field) {
    super(`${field} already exists`);
    this.field = field;
  }
}
