const request = require('supertest');
const assert = require('assert');

describe('Model Status and Parameter Config Tests', function() {
  const baseUrl = 'http://localhost:8082';

  describe('Model Status Check', function() {
    it('should check model status successfully', function(done) {
      const testData = {
        ip_address: '192.168.1.100'
      };

      request(baseUrl)
        .post('/api/model/status')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          // Check response structure
          assert(res.body.hasOwnProperty('success'), 'Response should have success property');

          done();
        });
    });

    it('should handle missing ip_address parameter', function(done) {
      const testData = {
        // Missing ip_address
      };

      request(baseUrl)
        .post('/api/model/status')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          done();
        });
    });
  });

  describe('Parameter Config', function() {
    it('should send parameter config successfully', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        config: {
          modelPath: '/home/chano/Github/dynamic_gestures',
          inferenceEngine: 'tensorrt',
          batchSize: 1,
          confidenceThreshold: 0.5,
          inputSize: 224,
          threads: 2,
          precision: 'int8',
          iouThreshold: 0.3,
          maxAge: 30,
          minHits: 3,
          fpsLimit: 0
        }
      };

      request(baseUrl)
        .post('/api/config/send')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          // Check response structure
          assert(res.body.hasOwnProperty('success'), 'Response should have success property');
          assert(res.body.hasOwnProperty('message'), 'Response should have message property');

          done();
        });
    });

    it('should handle missing config parameter', function(done) {
      const testData = {
        ip_address: '192.168.1.100'
        // Missing config
      };

      request(baseUrl)
        .post('/api/config/send')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          done();
        });
    });

    it('should validate config structure', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        config: {
          // Missing required fields
          batchSize: 1
        }
      };

      request(baseUrl)
        .post('/api/config/send')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          done();
        });
    });
  });

  describe('Parameter Config Validation', function() {
    it('should validate batchSize parameter', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        config: {
          modelPath: '/home/chano/Github/dynamic_gestures',
          inferenceEngine: 'tensorrt',
          batchSize: 0, // Invalid batch size
          confidenceThreshold: 0.5,
          inputSize: 224,
          threads: 2,
          precision: 'int8',
          iouThreshold: 0.3,
          maxAge: 30,
          minHits: 3,
          fpsLimit: 0
        }
      };

      request(baseUrl)
        .post('/api/config/send')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          done();
        });
    });

    it('should validate confidenceThreshold parameter', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        config: {
          modelPath: '/home/chano/Github/dynamic_gestures',
          inferenceEngine: 'tensorrt',
          batchSize: 1,
          confidenceThreshold: 1.5, // Invalid confidence threshold
          inputSize: 224,
          threads: 2,
          precision: 'int8',
          iouThreshold: 0.3,
          maxAge: 30,
          minHits: 3,
          fpsLimit: 0
        }
      };

      request(baseUrl)
        .post('/api/config/send')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          done();
        });
    });
  });
});
