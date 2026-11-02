// Sends the verification e-mails. Providers: smtp (any SMTP service), resend (HTTP API), console (development: prints the
// e-mail, including the code, to the log), memory (tests).
import nodemailer from 'nodemailer';

export function codeEmail({ code, minutes, purpose }) {
  const what = purpose === 'signup' ? 'finish creating your Boldinbeauty account' : 'log in to Boldinbeauty';
  const text = [
    `Your Boldinbeauty verification code is ${code}.`,
    '',
    `Enter it in the game to ${what}. It expires in ${minutes} minutes and works once.`,
    '',
    'If you did not ask for this, ignore this e-mail: nobody can use your account without the code.',
  ].join('\n');
  const html = `<p>Your Boldinbeauty verification code is</p><p style="font-size:28px;font-weight:bold;letter-spacing:6px">${code}</p>`
    + `<p>Enter it in the game to ${what}. It expires in ${minutes} minutes and works once.</p>`
    + '<p style="color:#666">If you did not ask for this, ignore this e-mail: nobody can use your account without the code.</p>';
  return { subject: `Your Boldinbeauty code: ${code}`, text, html };
}

export class MemoryMailer {
  constructor() {
    this.sent = [];
    this.failNext = false;
  }

  async send(message) {
    if (this.failNext) {
      this.failNext = false;
      throw new Error('simulated mail failure');
    }
    this.sent.push(message);
  }

  last() {
    return this.sent[this.sent.length - 1];
  }

  // The 6-digit code in the most recent e-mail to an address.
  lastCodeFor(to) {
    for (let index = this.sent.length - 1; index >= 0; index -= 1) {
      if (this.sent[index].to === to) return /\b(\d{6})\b/.exec(this.sent[index].text)[1];
    }
    return null;
  }
}

export class ConsoleMailer {
  constructor(log) {
    this.log = log;
  }

  async send(message) {
    this.log('warn', 'DEV e-mail (console provider)', { to: message.to, subject: message.subject, text: message.text });
  }
}

export class SmtpMailer {
  constructor(config, log) {
    this.from = config.from;
    this.log = log;
    const local = ['127.0.0.1', 'localhost', '::1'].includes(config.smtp.host);
    this.transport = nodemailer.createTransport({
      host: config.smtp.host,
      port: config.smtp.port,
      secure: config.smtp.secure,
      // On a plain port the login must be upgraded to TLS (STARTTLS); never fall back to sending the password in clear text.
      requireTLS: !config.smtp.secure && !local,
      auth: { user: config.smtp.user, pass: config.smtp.pass },
      connectionTimeout: 10_000,
      greetingTimeout: 10_000,
      socketTimeout: 15_000,
    });
  }

  async verify() {
    await this.transport.verify();
  }

  async send(message) {
    await this.transport.sendMail({ from: this.from, to: message.to, subject: message.subject, text: message.text, html: message.html });
  }
}

export class ResendMailer {
  constructor(config) {
    this.from = config.from;
    this.key = config.resendKey;
  }

  async send(message) {
    const response = await fetch('https://api.resend.com/emails', {
      method: 'POST',
      headers: { Authorization: `Bearer ${this.key}`, 'Content-Type': 'application/json' },
      body: JSON.stringify({ from: this.from, to: [message.to], subject: message.subject, text: message.text, html: message.html }),
      signal: AbortSignal.timeout(15_000),
    });
    if (!response.ok) throw new Error(`Resend answered HTTP ${response.status}`);
  }
}

export function createMailer(mailConfig, log) {
  switch (mailConfig.provider) {
    case 'smtp': return new SmtpMailer(mailConfig, log);
    case 'resend': return new ResendMailer(mailConfig);
    default: return new ConsoleMailer(log);
  }
}
