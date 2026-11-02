// The SMTP sender against a local SMTP server: proves the message that leaves is well formed.
import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';
import { simpleParser } from 'mailparser';
import { SMTPServer } from 'smtp-server';
import { SmtpMailer, codeEmail } from '../src/mailer.js';

describe('SMTP mailer', () => {
  let server; let port; const received = []; const logins = [];

  before(async () => {
    server = new SMTPServer({
      authOptional: false,
      allowInsecureAuth: true,
      disabledCommands: ['STARTTLS'],
      onAuth(auth, session, callback) {
        logins.push({ user: auth.username, ok: auth.username === 'mailuser' && auth.password === 'mailpass' });
        if (auth.username === 'mailuser' && auth.password === 'mailpass') callback(null, { user: 1 });
        else callback(new Error('bad credentials'));
      },
      onData(stream, session, callback) {
        simpleParser(stream).then((mail) => { received.push(mail); callback(); }, callback);
      },
    });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    port = server.server.address().port;
  });
  after(async () => { await new Promise((resolve) => server.close(resolve)); });

  const config = (overrides = {}) => ({
    from: 'Boldinbeauty <no-reply@example.com>',
    smtp: { host: '127.0.0.1', port, secure: false, user: 'mailuser', pass: 'mailpass', ...overrides },
  });

  it('delivers the code e-mail with the right sender, recipient and body', async () => {
    const mailer = new SmtpMailer(config(), () => {});
    await mailer.verify();
    await mailer.send({ to: 'nova@example.com', ...codeEmail({ code: '042517', minutes: 10, purpose: 'login' }) });
    assert.equal(received.length, 1);
    const mail = received[0];
    assert.equal(mail.to.value[0].address, 'nova@example.com');
    assert.equal(mail.from.value[0].address, 'no-reply@example.com');
    assert.match(mail.subject, /042517/);
    assert.match(mail.text, /042517/);
    assert.match(mail.html, /042517/);
  });

  it('reports bad credentials instead of pretending to send', async () => {
    const mailer = new SmtpMailer(config({ pass: 'wrong' }), () => {});
    await assert.rejects(mailer.send({ to: 'nova@example.com', subject: 's', text: 't', html: '<p>t</p>' }));
    assert.equal(received.length, 1, 'nothing more was delivered');
  });

  it('fails fast when the server is unreachable', async () => {
    const mailer = new SmtpMailer(config({ port: 1 }), () => {});
    await assert.rejects(mailer.send({ to: 'nova@example.com', subject: 's', text: 't', html: '<p>t</p>' }));
  });
});
