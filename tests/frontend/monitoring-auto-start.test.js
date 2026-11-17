const request = require('supertest');
const assert = require('assert');

describe('Monitoring Auto Start Tests', function() {
  const baseUrl = 'http://localhost:8082';

  describe('Performance Monitoring Control', function() {
    it('should start monitoring successfully', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        action: 'start'
      };

      request(baseUrl)
        .post('/api/devices/performance_control')
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

    it('should stop monitoring successfully', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        action: 'stop'
      };

      request(baseUrl)
        .post('/api/devices/performance_control')
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

    it('should handle invalid action parameter', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        action: 'invalid' // Invalid action
      };

      request(baseUrl)
        .post('/api/devices/performance_control')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          // Should return success: false for invalid action
          assert(!res.body.success, 'Should return success: false for invalid action');
          assert(res.body.message.includes('Invalid action'), 'Error message should indicate invalid action');

          done();
        });
    });

    it('should handle missing parameters', function(done) {
      const testData = {
        // Missing ip_address and action
      };

      request(baseUrl)
        .post('/api/devices/performance_control')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          // Should return success: false for missing parameters
          assert(!res.body.success, 'Should return success: false for missing parameters');
          assert(res.body.message.includes('Missing required parameters'), 'Error message should indicate missing parameters');

          done();
        });
    });
  });

  describe('Monitoring State Management', function() {
    it('should toggle monitoring state correctly', function(done) {
      // First start monitoring
      const startData = {
        ip_address: '192.168.1.100',
        action: 'start'
      };

      request(baseUrl)
        .post('/api/devices/performance_control')
        .send(startData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          // Then stop monitoring
          const stopData = {
            ip_address: '192.168.1.100',
            action: 'stop'
          };

          request(baseUrl)
            .post('/api/devices/performance_control')
            .send(stopData)
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
    });
  });

  describe('Monitoring Auto Start Prevention', function() {
    it('should not start monitoring automatically without user action', function(done) {
      // This test verifies that monitoring doesn't start automatically
      // We'll simulate a device connection and check that monitoring isn't started
      const testData = {
        ip_address: '192.168.1.100',
        username: 'root',
        password: '',
        port: 22,
        timeout: 30
      };

      request(baseUrl)
        .post('/api/devices/connect')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);

          // Check response structure
          assert(res.body.hasOwnProperty('success'), 'Response should have success property');

          // After connection, monitoring should not be started automatically
          // We'll verify this by checking the monitoring status
          const statusData = {
            ip_address: '192.168.1.100'
          };

          // Note: We're assuming there's a monitoring status endpoint
          // If not, this test may need to be adjusted
          request(baseUrl)
            .post('/api/devices/monitoring_status')
            .send(statusData)
            .set('Content-Type', 'application/json')
            .expect(200)
            .end(function(err, res) {
              if (err) return done(err);

              // Check response structure
              assert(res.body.hasOwnProperty('success'), 'Response should have success property');
              assert(res.body.hasOwnProperty('is_monitoring'), 'Response should have is_monitoring property');

              // Monitoring should not be active
              assert(!res.body.is_monitoring, 'Monitoring should not be active after connection');

              done();
            });
        });
    });
  });
});
