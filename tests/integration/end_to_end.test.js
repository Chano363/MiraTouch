const request = require('supertest');
const assert = require('assert');

describe('End-to-End Integration Tests', function() {
  const baseUrl = 'http://localhost:8082';
  
  describe('Configuration workflow', function() {
    it('should handle complete configuration workflow', function(done) {
      const deviceIp = '192.168.1.100';
      const username = 'chano';
      
      // Step 1: Send configuration
      const configData = {
        ip_address: deviceIp,
        username: username,
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
        .send(configData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res1) {
          if (err) return done(err);
          
          // Check send response
          assert(res1.body.hasOwnProperty('success'), 'Send response should have success property');
          assert(res1.body.hasOwnProperty('error'), 'Send response should have error property');
          assert(res1.body.hasOwnProperty('message'), 'Send response should have message property');
          
          // Step 2: Read configuration status
          const statusData = {
            ip_address: deviceIp,
            username: username
          };
          
          request(baseUrl)
            .post('/api/config/status')
            .send(statusData)
            .set('Content-Type', 'application/json')
            .expect(200)
            .end(function(err, res2) {
              if (err) return done(err);
              
              // Check status response
              assert(res2.body.hasOwnProperty('success'), 'Status response should have success property');
              assert(res2.body.hasOwnProperty('error'), 'Status response should have error property');
              assert(res2.body.hasOwnProperty('message'), 'Status response should have message property');
              assert(res2.body.hasOwnProperty('has_config'), 'Status response should have has_config property');
              
              // Check response consistency
              assert.strictEqual(typeof res1.body.success, typeof res2.body.success, 'Success property type should be consistent');
              assert.strictEqual(typeof res1.body.error, typeof res2.body.error, 'Error property type should be consistent');
              assert.strictEqual(typeof res1.body.message, typeof res2.body.message, 'Message property type should be consistent');
              
              // Check path consistency
              if (!res1.body.success && !res2.body.success) {
                assert(res1.body.error.includes(`/home/${username}/Github/dynamic_gestures`), 'Send error should include correct path');
                assert(res2.body.error.includes(`/home/${username}/Github/dynamic_gestures`), 'Status error should include correct path');
              }
              
              done();
            });
        });
    });
  });
  
  describe('Error handling consistency', function() {
    it('should handle device not connected error consistently', function(done) {
      const testData = {
        ip_address: '192.168.1.999', // Non-existent IP
        username: 'chano'
      };
      
      // Test send configuration
      request(baseUrl)
        .post('/api/config/send')
        .send({...testData, config: { batchSize: 1 }})
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res1) {
          if (err) return done(err);
          
          // Test read configuration status
          request(baseUrl)
            .post('/api/config/status')
            .send(testData)
            .set('Content-Type', 'application/json')
            .expect(200)
            .end(function(err, res2) {
              if (err) return done(err);
              
              // Both should fail
              assert(!res1.body.success, 'Send should fail for non-existent device');
              assert(!res2.body.success, 'Status should fail for non-existent device');
              
              // Both should return error messages
              assert(res1.body.error.length > 0, 'Send should return error message');
              assert(res2.body.error.length > 0, 'Status should return error message');
              
              // Both should include device IP in error message
              assert(res1.body.error.includes('192.168.1.999'), 'Send error should include device IP');
              assert(res2.body.error.includes('192.168.1.999'), 'Status error should include device IP');
              
              // Both should include username in path
              assert(res1.body.error.includes('/home/chano/Github/dynamic_gestures'), 'Send error should include correct path');
              assert(res2.body.error.includes('/home/chano/Github/dynamic_gestures'), 'Status error should include correct path');
              
              done();
            });
        });
    });
  });
  
  describe('Boundary cases', function() {
    it('should handle missing optional parameters', function(done) {
      const testData = {
        ip_address: '192.168.1.100'
        // Missing username parameter
      };
      
      request(baseUrl)
        .post('/api/config/status')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);
          
          // Should use default username
          if (!res.body.success) {
            assert(res.body.error.includes('/home/chano/Github/dynamic_gestures'), 'Error should include default username in path');
          }
          
          done();
        });
    });
    
    it('should handle different root_dir parameter', function(done) {
      const testData = {
        ip_address: '192.168.1.100',
        username: 'chano',
        root_dir: '/custom/path/to/project'
      };
      
      request(baseUrl)
        .post('/api/config/status')
        .send(testData)
        .set('Content-Type', 'application/json')
        .expect(200)
        .end(function(err, res) {
          if (err) return done(err);
          
          // Should use custom root_dir
          if (!res.body.success) {
            assert(res.body.error.includes('/custom/path/to/project'), 'Error should include custom root_dir');
          }
          
          done();
        });
    });
  });
});
