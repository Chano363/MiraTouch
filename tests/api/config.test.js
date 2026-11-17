const request = require('supertest');
const assert = require('assert');

describe('Config API Tests', function() {
  const baseUrl = 'http://localhost:8082';
  
  describe('POST /api/config/send', function() {
    it('should send configuration parameters successfully', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        username: 'chano',
        config: {
          batchSize: 2,
          inputSize: 224,
          threads: 4,
          precision: 'int8',
          confidence: 0.6,
          iou_threshold: 0.4,
          max_age: 35,
          min_hits: 4,
          fps_limit: 30
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
          assert(res.body.hasOwnProperty('error'), 'Response should have error property');
          assert(res.body.hasOwnProperty('message'), 'Response should have message property');
          
          // Check error message format
          if (!res.body.success) {
            assert(res.body.error.includes('Failed to execute command on device'), 'Error message should indicate command execution failure');
            assert(res.body.error.includes('192.168.1.100'), 'Error message should include device IP');
            assert(res.body.error.includes('/home/chano/Github/dynamic_gestures'), 'Error message should include correct path with username');
          }
          
          done();
        });
    });
    
    it('should handle missing configuration data', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        username: 'chano'
        // Missing config property
      };
      
      request(baseUrl)
        .post('/api/config/send')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);
          
          assert(!res.body.success, 'Should return success: false');
          assert(res.body.error.includes('Configuration data is missing'), 'Error message should indicate missing config data');
          done();
        });
    });
  });
  
  describe('POST /api/config/status', function() {
    it('should read configuration status successfully', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        username: 'chano'
      };
      
      request(baseUrl)
        .post('/api/config/status')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);
          
          // Check response structure
          assert(res.body.hasOwnProperty('success'), 'Response should have success property');
          assert(res.body.hasOwnProperty('error'), 'Response should have error property');
          assert(res.body.hasOwnProperty('message'), 'Response should have message property');
          assert(res.body.hasOwnProperty('has_config'), 'Response should have has_config property');
          
          // Check error message format
          if (!res.body.success) {
            assert(res.body.error.includes('Failed to execute command on device'), 'Error message should indicate command execution failure');
            assert(res.body.error.includes('192.168.1.100'), 'Error message should include device IP');
            assert(res.body.error.includes('/home/chano/Github/dynamic_gestures'), 'Error message should include correct path with username');
          }
          
          done();
        });
    });
    
    it('should handle different usernames correctly', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        username: 'testuser'
      };
      
      request(baseUrl)
        .post('/api/config/status')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);
          
          // Check error message includes correct username in path
          if (!res.body.success) {
            assert(res.body.error.includes('/home/testuser/Github/dynamic_gestures'), 'Error message should include correct path with username');
          }
          
          done();
        });
    });
  });
  
  describe('Response structure consistency', function() {
    it('should return consistent response structure for send and status endpoints', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        username: 'chano'
      };
      
      // Test send endpoint
      request(baseUrl)
        .post('/api/config/send')
        .send({...testData, config: { batchSize: 1 }})
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res1) {
          if (err) return done(err);
          
          // Test status endpoint
          request(baseUrl)
            .post('/api/config/status')
            .send(testData)
            .set('Content-Type', 'application/json')
            .expect(200)
            .end(function(err, res2) {
              if (err) return done(err);
              
              // Check both responses have the same structure
              const sendKeys = Object.keys(res1.body).sort();
              const statusKeys = Object.keys(res2.body).sort();
              
              // Both should have success, error, and message properties
              assert(sendKeys.includes('success'), 'Send response should have success property');
              assert(sendKeys.includes('error'), 'Send response should have error property');
              assert(sendKeys.includes('message'), 'Send response should have message property');
              
              assert(statusKeys.includes('success'), 'Status response should have success property');
              assert(statusKeys.includes('error'), 'Status response should have error property');
              assert(statusKeys.includes('message'), 'Status response should have message property');
              
              done();
            });
        });
    });
  });
});
